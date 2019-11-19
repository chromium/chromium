// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_handler_host_linux.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/linux_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"  // nogncheck
#include "third_party/breakpad/breakpad/src/client/linux/minidump_writer/linux_dumper.h"  // nogncheck
#include "third_party/breakpad/breakpad/src/client/linux/minidump_writer/minidump_writer.h"  // nogncheck
#endif  // ! defined(OS_ANDROID)

#if defined(OS_ANDROID) && !defined(__LP64__)
#include <sys/syscall.h>

#define SYS_read __NR_read
#endif

#if defined(OS_ANDROID)
#include "components/crash/content/app/crashpad.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"  // nogncheck
#include "third_party/crashpad/crashpad/util/posix/signals.h"      // nogncheck
#endif

using content::BrowserThread;

#if !defined(OS_ANDROID)

using google_breakpad::ExceptionHandler;

namespace breakpad {

namespace {

const size_t kNumFDs = 1;
// The length of the control message:
const size_t kControlMsgSize =
    CMSG_SPACE(kNumFDs * sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));
// The length of the regular payload:
const size_t kCrashContextSize = sizeof(ExceptionHandler::CrashContext);

// Crashing thread might be in "running" state, i.e. after sys_sendmsg() and
// before sys_read(). Retry 3 times with interval of 100 ms when translating
// TID.
const int kNumAttemptsTranslatingTid = 3;
const int kRetryIntervalTranslatingTidInMs = 100;

// Handles the crash dump and frees the allocated BreakpadInfo struct.
void CrashDumpTask(CrashHandlerHostLinux* handler,
                   std::unique_ptr<BreakpadInfo> info) {
  if (handler->IsShuttingDown() && info->upload) {
    base::DeleteFile(base::FilePath(info->filename), false);
#if defined(ADDRESS_SANITIZER)
    base::DeleteFile(base::FilePath(info->log_filename), false);
#endif
    return;
  }

  HandleCrashDump(*info);
  delete[] info->filename;
#if defined(ADDRESS_SANITIZER)
  delete[] info->log_filename;
  delete[] info->asan_report_str;
#endif
  delete[] info->process_type;
  delete[] info->distro;
  delete info->crash_keys;
}

}  // namespace

// Since instances of CrashHandlerHostLinux are leaked, they are only destroyed
// at the end of the processes lifetime, which is greater in span than the
// lifetime of the IO message loop. Thus, all calls to base::Bind() use
// non-refcounted pointers.

CrashHandlerHostLinux::CrashHandlerHostLinux(const std::string& process_type,
                                             const base::FilePath& dumps_path,
                                             bool upload)
    : process_type_(process_type),
      dumps_path_(dumps_path),
#if !defined(OS_ANDROID)
      upload_(upload),
#endif
      fd_watch_controller_(FROM_HERE),
      blocking_task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})) {
  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the process from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the process from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  static const int on = 1;

  // Enable passcred on the server end of the socket
  CHECK_EQ(0, setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)));

  process_socket_ = fds[0];
  browser_socket_ = fds[1];

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CrashHandlerHostLinux::Init, base::Unretained(this)));
}

CrashHandlerHostLinux::~CrashHandlerHostLinux() {
  close(process_socket_);
  close(browser_socket_);
}

void CrashHandlerHostLinux::StartUploaderThread() {
  uploader_thread_ =
      std::make_unique<base::Thread>(process_type_ + "_crash_uploader");
  uploader_thread_->Start();
}

void CrashHandlerHostLinux::Init() {
  base::MessageLoopCurrentForIO ml = base::MessageLoopCurrentForIO::Get();
  CHECK(ml->WatchFileDescriptor(browser_socket_, true /* persistent */,
                                base::MessagePumpForIO::WATCH_READ,
                                &fd_watch_controller_, this));
  ml->AddDestructionObserver(this);
}

void CrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void CrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(browser_socket_, fd);

  // A process has crashed and has signaled us by writing a datagram
  // to the death signal socket. The datagram contains the crash context needed
  // for writing the minidump as well as a file descriptor and a credentials
  // block so that they can't lie about their pid.
  //
  // The message sender is in components/crash/content/app/breakpad_linux.cc.

  struct msghdr msg = {nullptr};
  struct iovec iov[kCrashIovSize];

  auto crash_context = std::make_unique<char[]>(kCrashContextSize);
#if defined(ADDRESS_SANITIZER)
  auto asan_report = std::make_unique<char[]>(kMaxAsanReportSize + 1);
#endif

  auto crash_keys =
      std::make_unique<crash_reporter::internal::TransitionalCrashKeyStorage>();
  google_breakpad::SerializedNonAllocatingMap* serialized_crash_keys;
  size_t crash_keys_size = crash_keys->Serialize(
      const_cast<const google_breakpad::SerializedNonAllocatingMap**>(
          &serialized_crash_keys));

  char* tid_buf_addr = nullptr;
  int tid_fd = -1;
  uint64_t uptime;
  size_t oom_size;
  char control[kControlMsgSize];
  const ssize_t expected_msg_size =
      kCrashContextSize +
      sizeof(tid_buf_addr) + sizeof(tid_fd) +
      sizeof(uptime) +
#if defined(ADDRESS_SANITIZER)
      kMaxAsanReportSize + 1 +
#endif
      sizeof(oom_size) +
      crash_keys_size;
  iov[0].iov_base = crash_context.get();
  iov[0].iov_len = kCrashContextSize;
  iov[1].iov_base = &tid_buf_addr;
  iov[1].iov_len = sizeof(tid_buf_addr);
  iov[2].iov_base = &tid_fd;
  iov[2].iov_len = sizeof(tid_fd);
  iov[3].iov_base = &uptime;
  iov[3].iov_len = sizeof(uptime);
  iov[4].iov_base = &oom_size;
  iov[4].iov_len = sizeof(oom_size);
  iov[5].iov_base = serialized_crash_keys;
  iov[5].iov_len = crash_keys_size;
#if !defined(ADDRESS_SANITIZER)
  static_assert(5 == kCrashIovSize - 1, "kCrashIovSize should equal 6");
#else
  iov[6].iov_base = asan_report.get();
  iov[6].iov_len = kMaxAsanReportSize + 1;
  static_assert(6 == kCrashIovSize - 1, "kCrashIovSize should equal 7");
#endif
  msg.msg_iov = iov;
  msg.msg_iovlen = kCrashIovSize;
  msg.msg_control = control;
  msg.msg_controllen = kControlMsgSize;

  const ssize_t msg_size = HANDLE_EINTR(recvmsg(browser_socket_, &msg, 0));
  if (msg_size < 0) {
    PLOG(ERROR) << "Error reading from death signal socket. Crash dumping"
                << " is disabled."
                << " msg_size:" << msg_size;
    fd_watch_controller_.StopWatchingFileDescriptor();
    return;
  }
  const bool bad_message = (msg_size != expected_msg_size ||
                            msg.msg_controllen != kControlMsgSize ||
                            msg.msg_flags & ~MSG_TRUNC);
  base::ScopedFD signal_fd;
  pid_t crashing_pid = -1;
  if (msg.msg_controllen > 0) {
    // Walk the control payload and extract the file descriptor and
    // validated pid.
    for (struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg); hdr;
         hdr = CMSG_NXTHDR(&msg, hdr)) {
      if (hdr->cmsg_level != SOL_SOCKET)
        continue;
      if (hdr->cmsg_type == SCM_RIGHTS) {
        const size_t len = hdr->cmsg_len -
            (((uint8_t*)CMSG_DATA(hdr)) - (uint8_t*)hdr);
        DCHECK_EQ(0U, len % sizeof(int));
        const size_t num_fds = len / sizeof(int);
        if (num_fds != kNumFDs) {
          // A nasty process could try and send us too many descriptors and
          // force a leak.
          LOG(ERROR) << "Death signal contained wrong number of descriptors;"
                     << " num_fds:" << num_fds;
          for (size_t i = 0; i < num_fds; ++i)
            close(reinterpret_cast<int*>(CMSG_DATA(hdr))[i]);
          return;
        }
        DCHECK(!signal_fd.is_valid());
        int fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[0];
        DCHECK_GE(fd, 0);  // The kernel should never send a negative fd.
        signal_fd.reset(fd);
      } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
        DCHECK_EQ(-1, crashing_pid);
        const struct ucred *cred =
            reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
        crashing_pid = cred->pid;
      }
    }
  }

  if (bad_message) {
    LOG(ERROR) << "Received death signal message with the wrong size;"
               << " msg.msg_controllen:" << msg.msg_controllen
               << " msg.msg_flags:" << msg.msg_flags
               << " kCrashContextSize:" << kCrashContextSize
               << " kControlMsgSize:" << kControlMsgSize;
    return;
  }
  if (crashing_pid == -1 || !signal_fd.is_valid()) {
    LOG(ERROR) << "Death signal message didn't contain all expected control"
               << " messages";
    return;
  }

  // The crashing TID set inside the compromised context via
  // sys_gettid() in ExceptionHandler::HandleSignal might be wrong (if
  // the kernel supports PID namespacing) and may need to be
  // translated.
  //
  // We expect the crashing thread to be in sys_read(), waiting for us to
  // write to |signal_fd|. Most newer kernels where we have the different pid
  // namespaces also have /proc/[pid]/syscall, so we can look through
  // |actual_crashing_pid|'s thread group and find the thread that's in the
  // read syscall with the right arguments.

  std::string expected_syscall_data;
  // /proc/[pid]/syscall is formatted as follows:
  // syscall_number arg1 ... arg6 sp pc
  // but we just check syscall_number through arg3.
  base::StringAppendF(&expected_syscall_data, "%d 0x%x %p 0x1 ",
                      SYS_read, tid_fd, tid_buf_addr);

  FindCrashingThreadAndDump(crashing_pid,
                            expected_syscall_data,
                            std::move(crash_context),
                            std::move(crash_keys),
#if defined(ADDRESS_SANITIZER)
                            std::move(asan_report),
#endif
                            uptime,
                            oom_size,
                            signal_fd.release(),
                            0);
}

void CrashHandlerHostLinux::FindCrashingThreadAndDump(
    pid_t crashing_pid,
    const std::string& expected_syscall_data,
    std::unique_ptr<char[]> crash_context,
    std::unique_ptr<crash_reporter::internal::TransitionalCrashKeyStorage>
        crash_keys,
#if defined(ADDRESS_SANITIZER)
    std::unique_ptr<char[]> asan_report,
#endif
    uint64_t uptime,
    size_t oom_size,
    int signal_fd,
    int attempt) {
  bool syscall_supported = false;
  pid_t crashing_tid = base::FindThreadIDWithSyscall(
      crashing_pid, expected_syscall_data, &syscall_supported);
  ++attempt;
  if (crashing_tid == -1 && syscall_supported &&
      attempt <= kNumAttemptsTranslatingTid) {
    LOG(WARNING) << "Could not translate tid, attempt = " << attempt
                 << " retry ...";
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CrashHandlerHostLinux::FindCrashingThreadAndDump,
                       base::Unretained(this), crashing_pid,
                       expected_syscall_data, std::move(crash_context),
                       std::move(crash_keys),
#if defined(ADDRESS_SANITIZER)
                       std::move(asan_report),
#endif
                       uptime, oom_size, signal_fd, attempt),
        base::TimeDelta::FromMilliseconds(kRetryIntervalTranslatingTidInMs));
    return;
  }


  if (crashing_tid == -1) {
    // We didn't find the thread we want. Maybe it didn't reach
    // sys_read() yet or the thread went away.  We'll just take a
    // guess here and assume the crashing thread is the thread group
    // leader.  If procfs syscall is not supported by the kernel, then
    // we assume the kernel also does not support TID namespacing and
    // trust the TID passed by the crashing process.
    LOG(WARNING) << "Could not translate tid - assuming crashing thread is "
        "thread group leader; syscall_supported=" << syscall_supported;
    crashing_tid = crashing_pid;
  }

  ExceptionHandler::CrashContext* bad_context =
      reinterpret_cast<ExceptionHandler::CrashContext*>(crash_context.get());
  bad_context->tid = crashing_tid;

  auto info = std::make_unique<BreakpadInfo>();
  info->fd = -1;
  info->process_type_length = process_type_.length();
  // Freed in CrashDumpTask().
  char* process_type_str = new char[info->process_type_length + 1];
  process_type_.copy(process_type_str, info->process_type_length);
  process_type_str[info->process_type_length] = '\0';
  info->process_type = process_type_str;

  // Memory released from std::unique_ptrs below are also freed in
  // CrashDumpTask().
  info->crash_keys = crash_keys.release();
#if defined(ADDRESS_SANITIZER)
  asan_report[kMaxAsanReportSize] = '\0';
  info->asan_report_str = asan_report.release();
  info->asan_report_length = strlen(info->asan_report_str);
#endif

  info->process_start_time = uptime;
  info->oom_size = oom_size;
#if defined(OS_ANDROID)
  // Nothing gets uploaded in android.
  info->upload = false;
#else
  info->upload = upload_;
#endif

  BreakpadInfo* info_ptr = info.get();
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CrashHandlerHostLinux::WriteDumpFile,
                     base::Unretained(this), info_ptr, std::move(crash_context),
                     crashing_pid),
      base::BindOnce(&CrashHandlerHostLinux::QueueCrashDumpTask,
                     base::Unretained(this), std::move(info), signal_fd));
}

void CrashHandlerHostLinux::WriteDumpFile(BreakpadInfo* info,
                                          std::unique_ptr<char[]> crash_context,
                                          pid_t crashing_pid) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Set |info->distro| here because base::GetLinuxDistro() needs to run on a
  // blocking sequence.
  std::string distro = base::GetLinuxDistro();
  info->distro_length = distro.length();
  // Freed in CrashDumpTask().
  char* distro_str = new char[info->distro_length + 1];
  distro.copy(distro_str, info->distro_length);
  distro_str[info->distro_length] = '\0';
  info->distro = distro_str;

  base::FilePath dumps_path("/tmp");
  base::PathService::Get(base::DIR_TEMP, &dumps_path);
  if (!info->upload)
    dumps_path = dumps_path_;
  const std::string minidump_filename =
      base::StringPrintf("%s/chromium-%s-minidump-%016" PRIx64 ".dmp",
                         dumps_path.value().c_str(),
                         process_type_.c_str(),
                         base::RandUint64());

  if (!google_breakpad::WriteMinidump(minidump_filename.c_str(),
                                      kMaxMinidumpFileSize,
                                      crashing_pid,
                                      crash_context.get(),
                                      kCrashContextSize,
                                      google_breakpad::MappingList(),
                                      google_breakpad::AppMemoryList())) {
    LOG(ERROR) << "Failed to write crash dump for pid " << crashing_pid;
  }
#if defined(ADDRESS_SANITIZER)
  // Create a temporary file holding the AddressSanitizer report.
  const base::FilePath log_path =
      base::FilePath(minidump_filename).ReplaceExtension("log");
  base::WriteFile(log_path, info->asan_report_str, info->asan_report_length);
#endif

  // Freed in CrashDumpTask().
  char* minidump_filename_str = new char[minidump_filename.length() + 1];
  minidump_filename.copy(minidump_filename_str, minidump_filename.length());
  minidump_filename_str[minidump_filename.length()] = '\0';
  info->filename = minidump_filename_str;
#if defined(ADDRESS_SANITIZER)
  // Freed in CrashDumpTask().
  char* minidump_log_filename_str = new char[minidump_filename.length() + 1];
  minidump_filename.copy(minidump_log_filename_str, minidump_filename.length());
  memcpy(minidump_log_filename_str + minidump_filename.length() - 3, "log", 3);
  minidump_log_filename_str[minidump_filename.length()] = '\0';
  info->log_filename = minidump_log_filename_str;
#endif
  info->pid = crashing_pid;
}

void CrashHandlerHostLinux::QueueCrashDumpTask(
    std::unique_ptr<BreakpadInfo> info,
    int signal_fd) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Send the done signal to the process: it can exit now.
  struct msghdr msg = {nullptr};
  struct iovec done_iov;
  done_iov.iov_base = const_cast<char*>("\x42");
  done_iov.iov_len = 1;
  msg.msg_iov = &done_iov;
  msg.msg_iovlen = 1;

  HANDLE_EINTR(sendmsg(signal_fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL));
  close(signal_fd);

  uploader_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CrashDumpTask, base::Unretained(this), std::move(info)));
}

void CrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
  fd_watch_controller_.StopWatchingFileDescriptor();

  // If we are quitting and there are crash dumps in the queue, turn them into
  // no-ops.
  shutting_down_.Set();
  uploader_thread_->Stop();
}

bool CrashHandlerHostLinux::IsShuttingDown() const {
  return shutting_down_.IsSet();
}

}  // namespace breakpad

#else  // !OS_ANDROID

namespace crashpad {

void CrashHandlerHost::AddObserver(Observer* observer) {
  base::AutoLock lock(observers_lock_);
  bool inserted = observers_.insert(observer).second;
  DCHECK(inserted);
}

void CrashHandlerHost::RemoveObserver(Observer* observer) {
  base::AutoLock lock(observers_lock_);
  size_t removed = observers_.erase(observer);
  DCHECK(removed);
}

// static
CrashHandlerHost* CrashHandlerHost::Get() {
  static CrashHandlerHost* instance = new CrashHandlerHost();
  return instance;
}

int CrashHandlerHost::GetDeathSignalSocket() {
  static bool initialized = base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CrashHandlerHost::Init, base::Unretained(this)));
  DCHECK(initialized);

  return process_socket_.get();
}

CrashHandlerHost::~CrashHandlerHost() = default;

CrashHandlerHost::CrashHandlerHost()
    : observers_lock_(),
      observers_(),
      fd_watch_controller_(FROM_HERE),
      process_socket_(),
      browser_socket_() {
  int fds[2];
  // We use SOCK_SEQPACKET rather than SOCK_DGRAM to prevent the process from
  // sending datagrams to other sockets on the system. The sandbox may prevent
  // the process from calling socket() to create new sockets, but it'll still
  // inherit some sockets. With PF_UNIX+SOCK_DGRAM, it can call sendmsg to send
  // a datagram to any (abstract) socket on the same system. With
  // SOCK_SEQPACKET, this is prevented.
  CHECK_EQ(0, socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds));
  process_socket_.reset(fds[0]);
  browser_socket_.reset(fds[1]);

  static const int on = 1;
  CHECK_EQ(0, setsockopt(browser_socket_.get(), SOL_SOCKET, SO_PASSCRED, &on,
                         sizeof(on)));
}

void CrashHandlerHost::Init() {
  base::MessageLoopCurrentForIO ml = base::MessageLoopCurrentForIO::Get();
  CHECK(ml->WatchFileDescriptor(browser_socket_.get(), /* persistent= */ true,
                                base::MessagePumpForIO::WATCH_READ,
                                &fd_watch_controller_, this));
  ml->AddDestructionObserver(this);
}

bool CrashHandlerHost::ReceiveClientMessage(int client_fd,
                                            base::ScopedFD* handler_fd) {
  int signo;
  unsigned char request_dump;
  iovec iov[2];
  iov[0].iov_base = &signo;
  iov[0].iov_len = sizeof(signo);
  iov[1].iov_base = &request_dump;
  iov[1].iov_len = sizeof(request_dump);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = iov;
  msg.msg_iovlen = base::size(iov);

  char cmsg_buf[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(ucred))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  msg.msg_flags = 0;

  const ssize_t msg_size = HANDLE_EINTR(recvmsg(client_fd, &msg, 0));
  if (msg_size < 0) {
    PLOG(ERROR) << "recvmsg";
    return false;
  }

  base::ScopedFD child_fd;
  pid_t child_pid = -1;
  for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level != SOL_SOCKET) {
      continue;
    }

    if (cmsg->cmsg_type == SCM_RIGHTS) {
      child_fd.reset(*reinterpret_cast<int*>(CMSG_DATA(cmsg)));
    } else if (cmsg->cmsg_type == SCM_CREDENTIALS) {
      child_pid = reinterpret_cast<ucred*>(CMSG_DATA(cmsg))->pid;
    }
  }

  if (!child_fd.is_valid()) {
    LOG(ERROR) << "Death signal missing descriptor";
    return false;
  }

  if (child_pid < 0) {
    LOG(ERROR) << "Death signal missing pid";
    return false;
  }

  if (signo != crashpad::Signals::kSimulatedSigno) {
    NotifyCrashSignalObservers(child_pid, signo);
  }

  if (!request_dump) {
    return false;
  }

  handler_fd->reset(child_fd.release());
  return true;
}

void CrashHandlerHost::NotifyCrashSignalObservers(base::ProcessId pid,
                                                  int signo) {
  base::AutoLock lock(observers_lock_);
  for (Observer* observer : observers_) {
    observer->ChildReceivedCrashSignal(pid, signo);
  }
}

void CrashHandlerHost::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void CrashHandlerHost::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(browser_socket_.get(), fd);

  base::ScopedFD handler_fd;
  if (!ReceiveClientMessage(fd, &handler_fd)) {
    return;
  }

  bool result =
      crash_reporter::internal::StartHandlerForClient(handler_fd.get());
  DCHECK(result);
}

void CrashHandlerHost::WillDestroyCurrentMessageLoop() {
  fd_watch_controller_.StopWatchingFileDescriptor();
}

}  // namespace crashpad

#endif  // !OS_ANDROID
