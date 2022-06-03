// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#include <fcntl.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "build/branding_buildflags.h"
#include "chrome/common/multi_process_lock.h"

#if defined(OS_ANDROID)
#error "Should not be built on android"
#endif

namespace {
int g_signal_socket = -1;

#if !defined(OS_MAC)

bool FilePathForMemoryName(const std::string& mem_name, base::FilePath* path) {
  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK_EQ(std::string::npos, mem_name.find('/'));
  DCHECK_EQ(std::string::npos, mem_name.find('\0'));

  base::FilePath temp_dir;
  if (!GetShmemTempDir(false, &temp_dir))
    return false;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const char kShmem[] = "com.google.Chrome.shmem.";
#else
  static const char kShmem[] = "org.chromium.Chromium.shmem.";
#endif
  *path = temp_dir.AppendASCII(kShmem + mem_name);
  return true;
}

#endif  // !defined(OS_MAC)

}  // namespace

#if !defined(OS_MAC)

// static
base::WritableSharedMemoryRegion
ServiceProcessState::CreateServiceProcessDataRegion(size_t size) {
  base::FilePath path;

  if (!FilePathForMemoryName(GetServiceProcessSharedMemName(), &path))
    return {};

  // Make sure that the file is opened without any permission
  // to other users on the system.
  const mode_t kOwnerOnly = S_IRUSR | S_IWUSR;

  bool fix_size = true;

  // First, try to create the file.
  base::ScopedFD fd(HANDLE_EINTR(
      open(path.value().c_str(), O_RDWR | O_CREAT | O_EXCL, kOwnerOnly)));
  if (!fd.is_valid()) {
    // If this doesn't work, try and open an existing file in append mode.
    // Opening an existing file in a world writable directory has two main
    // security implications:
    // - Attackers could plant a file under their control, so ownership of
    //   the file is checked below.
    // - Attackers could plant a symbolic link so that an unexpected file
    //   is opened, so O_NOFOLLOW is passed to open().
#if !defined(OS_AIX)
    fd.reset(HANDLE_EINTR(
        open(path.value().c_str(), O_RDWR | O_APPEND | O_NOFOLLOW)));
#else
    // AIX has no 64-bit support for open flags such as -
    //  O_CLOEXEC, O_NOFOLLOW and O_TTY_INIT.
    fd.reset(HANDLE_EINTR(open(path.value().c_str(), O_RDWR | O_APPEND)));
#endif
    // Check that the current user owns the file.
    // If uid != euid, then a more complex permission model is used and this
    // API is not appropriate.
    const uid_t real_uid = getuid();
    const uid_t effective_uid = geteuid();
    struct stat sb;
    if (fd.is_valid() && (fstat(fd.get(), &sb) != 0 || sb.st_uid != real_uid ||
                          sb.st_uid != effective_uid)) {
      DLOG(ERROR) << "Invalid owner when opening existing shared memory file.";
      return {};
    }

    // An existing file was opened, so its size should not be fixed.
    fix_size = false;
  }

  if (fd.is_valid() && fix_size) {
    // Get current size.
    struct stat stat;
    if (fstat(fd.get(), &stat) != 0)
      return {};
    const size_t current_size = stat.st_size;
    if (current_size != size) {
      if (HANDLE_EINTR(ftruncate(fd.get(), size)) != 0)
        return {};
    }
  }

  // Everything has worked out so far, so open a read-only handle to the region
  // in order to be able to create a writable region (which needs a read-only
  // handle in order to convert to a read-only region.
  base::ScopedFD read_only_fd(
      HANDLE_EINTR(open(path.value().c_str(), O_RDONLY, kOwnerOnly)));
  if (!read_only_fd.is_valid()) {
    DPLOG(ERROR) << "Could not reopen shared memory region as read-only";
    return {};
  }

  base::WritableSharedMemoryRegion writable_region =
      base::WritableSharedMemoryRegion::Deserialize(
          base::subtle::PlatformSharedMemoryRegion::Take(
              base::subtle::ScopedFDPair(std::move(fd),
                                         std::move(read_only_fd)),
              base::subtle::PlatformSharedMemoryRegion::Mode::kWritable, size,
              base::UnguessableToken::Create()));
  if (!writable_region.IsValid()) {
    DLOG(ERROR) << "Could not deserialize named region";
    return {};
  }
  return writable_region;
}

// static
base::ReadOnlySharedMemoryMapping
ServiceProcessState::OpenServiceProcessDataMapping(size_t size) {
  base::FilePath path;
  if (!FilePathForMemoryName(GetServiceProcessSharedMemName(), &path))
    return {};

  base::ScopedFD fd(HANDLE_EINTR(open(path.value().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DPLOG(ERROR) << "open(\"" << path.value() << "\", O_RDONLY) failed";
    return {};
  }
  return base::ReadOnlySharedMemoryRegion::Deserialize(
             base::subtle::PlatformSharedMemoryRegion::Take(
                 base::subtle::ScopedFDPair(std::move(fd), base::ScopedFD()),
                 base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
                 size, base::UnguessableToken::Create()))
      .Map();
}

// static
bool ServiceProcessState::DeleteServiceProcessDataRegion() {
  base::FilePath path;
  if (!FilePathForMemoryName(GetServiceProcessSharedMemName(), &path))
    return false;

  if (PathExists(path))
    return base::DeleteFile(path);

  // Doesn't exist, so success.
  return true;
}

#endif  // !defined(OS_MAC)

// Attempts to take a lock named |name|. Returns the lock if successful, or
// nullptr if not.
std::unique_ptr<MultiProcessLock> TakeNamedLock(const std::string& name) {
  std::unique_ptr<MultiProcessLock> lock = MultiProcessLock::Create(name);
  if (!lock->TryLock())
    lock.reset();
  return lock;
}

ServiceProcessTerminateMonitor::ServiceProcessTerminateMonitor(
    base::OnceClosure terminate_task)
    : terminate_task_(std::move(terminate_task)) {}

ServiceProcessTerminateMonitor::~ServiceProcessTerminateMonitor() {
}

void ServiceProcessTerminateMonitor::OnFileCanReadWithoutBlocking(int fd) {
  if (!terminate_task_.is_null()) {
    int buffer;
    int length = read(fd, &buffer, sizeof(buffer));
    if ((length == sizeof(buffer)) && (buffer == kTerminateMessage)) {
      std::move(terminate_task_).Run();
    } else if (length > 0) {
      DLOG(ERROR) << "Unexpected read: " << buffer;
    } else if (length == 0) {
      DLOG(ERROR) << "Unexpected fd close";
    } else if (length < 0) {
      DPLOG(ERROR) << "read";
    }
  }
}

void ServiceProcessTerminateMonitor::OnFileCanWriteWithoutBlocking(int fd) {
  NOTIMPLEMENTED();
}

// "Forced" Shutdowns on POSIX are done via signals. The magic signal for
// a shutdown is SIGTERM. "write" is a signal safe function. PLOG(ERROR) is
// not, but we don't ever expect it to be called.
static void SigTermHandler(int sig, siginfo_t* info, void* uap) {
  // TODO(dmaclach): add security here to make sure that we are being shut
  //                 down by an appropriate process.
  int message = ServiceProcessTerminateMonitor::kTerminateMessage;
  if (write(g_signal_socket, &message, sizeof(message)) < 0) {
    DPLOG(ERROR) << "write";
  }
}

ServiceProcessState::StateData::StateData()
    : watcher(FROM_HERE), set_action(false) {
  memset(sockets, -1, sizeof(sockets));
  memset(&old_action, 0, sizeof(old_action));
}

void ServiceProcessState::StateData::SignalReady(base::WaitableEvent* signal,
                                                 bool* success) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK_EQ(g_signal_socket, -1);
  DCHECK(!signal->IsSignaled());
  *success = base::CurrentIOThread::Get()->WatchFileDescriptor(
      sockets[0], true, base::MessagePumpForIO::WATCH_READ, &watcher,
      terminate_monitor.get());
  if (!*success) {
    DLOG(ERROR) << "WatchFileDescriptor";
    signal->Signal();
    return;
  }
  g_signal_socket = sockets[1];

  // Set up signal handler for SIGTERM.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_sigaction = SigTermHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  *success = sigaction(SIGTERM, &action, &old_action) == 0;
  if (!*success) {
    DPLOG(ERROR) << "sigaction";
    signal->Signal();
    return;
  }

  // If the old_action is not default, somebody else has installed a
  // a competing handler. Our handler is going to override it so it
  // won't be called. If this occurs it needs to be fixed.
  DCHECK_EQ(old_action.sa_handler, SIG_DFL);
  set_action = true;

#if defined(OS_MAC)
  *success = WatchExecutable();
  if (!*success) {
    DLOG(ERROR) << "WatchExecutable";
    signal->Signal();
    return;
  }
#elif defined(OS_POSIX)
  initializing_lock.reset();
#endif  // OS_POSIX
  signal->Signal();
}

ServiceProcessState::StateData::~StateData() {
  // StateData is destroyed on the thread that called SignalReady() (if any) to
  // satisfy the requirement that base::FilePathWatcher is destroyed in sequence
  // with base::FilePathWatcher::Watch().
  DCHECK(!task_runner || task_runner->BelongsToCurrentThread());

  // Cancel any pending file-descriptor watch before closing the descriptor.
  watcher.StopWatchingFileDescriptor();

  if (sockets[0] != -1) {
    if (IGNORE_EINTR(close(sockets[0]))) {
      DPLOG(ERROR) << "close";
    }
  }
  if (sockets[1] != -1) {
    if (IGNORE_EINTR(close(sockets[1]))) {
      DPLOG(ERROR) << "close";
    }
  }
  if (set_action) {
    if (sigaction(SIGTERM, &old_action, NULL) < 0) {
      DPLOG(ERROR) << "sigaction";
    }
  }
  g_signal_socket = -1;
}

void ServiceProcessState::CreateState() {
  DCHECK(!state_);
  state_ = new StateData();
}

bool ServiceProcessState::SignalReady(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure terminate_task) {
  DCHECK(task_runner);
  DCHECK(state_);

#if !defined(OS_MAC)
  state_->running_lock = TakeServiceRunningLock();
  if (!state_->running_lock.get()) {
    return false;
  }
#endif
  state_->terminate_monitor = std::make_unique<ServiceProcessTerminateMonitor>(
      std::move(terminate_task));
  if (pipe(state_->sockets) < 0) {
    DPLOG(ERROR) << "pipe";
    return false;
  }
  base::WaitableEvent signal_ready(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool success = false;

  state_->task_runner = std::move(task_runner);
  state_->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceProcessState::StateData::SignalReady,
                     base::Unretained(state_), &signal_ready, &success));
  signal_ready.Wait();
  return success;
}

void ServiceProcessState::TearDownState() {
  if (state_ && state_->task_runner)
    state_->task_runner->DeleteSoon(FROM_HERE, state_);
  else
    delete state_;
  state_ = nullptr;
}
