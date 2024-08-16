// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/process_proxy/process_proxy.h"

#include <stddef.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <utility>

#include "base/command_line.h"
#include "base/file_descriptor_posix.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace {

enum PseudoTerminalFd {
  PT_MASTER_FD,
  PT_SLAVE_FD
};

void StopOutputWatcher(
    std::unique_ptr<chromeos::ProcessOutputWatcher> watcher) {
  // Just deleting |watcher| if sufficient to stop it.
}

}  // namespace

namespace chromeos {

ProcessProxy::ProcessProxy() : process_launched_(false), callback_set_(false) {
  // Set pipes to initial, invalid value so we can easily know if a pipe was
  // opened by us.
  ClearFdPair(pt_pair_);
}

bool ProcessProxy::Open(const base::CommandLine& cmdline,
                        const std::string& user_id_hash,
                        std::string* id) {
  if (process_launched_)
    return false;

  if (!CreatePseudoTerminalPair(pt_pair_)) {
    return false;
  }

  process_launched_ =
      LaunchProcess(cmdline, user_id_hash, pt_pair_[PT_SLAVE_FD], id);

  if (process_launched_) {
    CloseFd(&pt_pair_[PT_SLAVE_FD]);
  } else {
    CloseFdPair(pt_pair_);
  }
  return process_launched_;
}

bool ProcessProxy::StartWatchingOutput(
    const scoped_refptr<base::SingleThreadTaskRunner>& watcher_runner,
    const scoped_refptr<base::SequencedTaskRunner>& callback_runner,
    const OutputCallback& callback) {
  DCHECK(process_launched_);
  CHECK(!output_watcher_.get());

  // We give ProcessOutputWatcher a copy of master to make life easier during
  // tear down.
  // TODO(tbarzic): improve fd managment.
  int master_copy = HANDLE_EINTR(dup(pt_pair_[PT_MASTER_FD]));
  if (master_copy < 0)
    return false;

  callback_set_ = true;
  callback_ = callback;
  callback_runner_ = callback_runner;
  watcher_runner_ = watcher_runner;

  // This object will delete itself once watching is stopped.
  // It also takes ownership of the passed fds.
  output_watcher_ = std::make_unique<ProcessOutputWatcher>(
      master_copy, base::BindRepeating(&ProcessProxy::OnProcessOutput, this));

  watcher_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProcessOutputWatcher::Start,
                                base::Unretained(output_watcher_.get())));

  return true;
}

void ProcessProxy::OnProcessOutput(ProcessOutputType type,
                                   const std::string& output) {
  if (!callback_runner_.get())
    return;

  callback_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProcessProxy::CallOnProcessOutputCallback,
                                this, type, output));
}

void ProcessProxy::CallOnProcessOutputCallback(ProcessOutputType type,
                                               const std::string& output) {
  // We may receive some output even after Close was called (crosh process does
  // not have to quit instantly, or there may be some trailing data left in
  // output stream fds). In that case owner of the callback may be gone so we
  // don't want to send it anything. |callback_set_| is reset when this gets
  // closed.
  if (callback_set_) {
    callback_.Run(type, output);
  }
}

void ProcessProxy::AckOutput() {
  watcher_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&ProcessOutputWatcher::AckOutput,
                                           output_watcher_->GetWeakPtr()));
}

void ProcessProxy::StopWatching() {
  if (!output_watcher_.get())
    return;

  watcher_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StopOutputWatcher, std::move(output_watcher_)));
}

void ProcessProxy::Close() {
  if (!process_launched_)
    return;

  process_launched_ = false;
  callback_set_ = false;
  callback_.Reset();
  callback_runner_.reset();

  process_.Terminate(0, /* wait */ false);
  base::EnsureProcessTerminated(std::move(process_));

  StopWatching();
  CloseFdPair(pt_pair_);
}

void ProcessProxy::Write(const std::string& text,
                         base::OnceCallback<void(bool)> callback) {
  if (!process_launched_)
    return std::move(callback).Run(false);

  // Use ThreadPool for write to avoid deadlock on registry TaskRunner.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](int fd, const std::string& text) {
            return base::WriteFileDescriptor(fd, text);
          },
          pt_pair_[PT_MASTER_FD], text),
      std::move(callback));
}

bool ProcessProxy::OnTerminalResize(int width, int height) {
  if (width < 0 || height < 0)
    return false;

  winsize ws;
  // Number of rows.
  ws.ws_row = height;
  // Number of columns.
  ws.ws_col = width;

  return (HANDLE_EINTR(ioctl(pt_pair_[PT_MASTER_FD], TIOCSWINSZ, &ws)) != -1);
}

ProcessProxy::~ProcessProxy() {
  Close();
}

bool ProcessProxy::CreatePseudoTerminalPair(int *pt_pair) {
  ClearFdPair(pt_pair);

  // Open Master.
  pt_pair[PT_MASTER_FD] = HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY));
  if (pt_pair[PT_MASTER_FD] == -1)
    return false;

  if (grantpt(pt_pair_[PT_MASTER_FD]) != 0 ||
      unlockpt(pt_pair_[PT_MASTER_FD]) != 0) {
    CloseFd(&pt_pair[PT_MASTER_FD]);
    return false;
  }
  char* slave_name = NULL;
  // Per man page, slave_name must not be freed.
  slave_name = ptsname(pt_pair_[PT_MASTER_FD]);
  if (slave_name)
    pt_pair_[PT_SLAVE_FD] = HANDLE_EINTR(open(slave_name, O_RDWR | O_NOCTTY));

  if (pt_pair_[PT_SLAVE_FD] == -1) {
    CloseFdPair(pt_pair);
    return false;
  }

  // Get the current tty settings so we can overlay our updates.
  struct termios termios;
  if (tcgetattr(pt_pair_[PT_SLAVE_FD], &termios) != 0) {
    CloseFdPair(pt_pair);
    return false;
  }

  // Set the IUTF8 bit on the tty as we should be UTF-8 clean everywhere.
  termios.c_iflag |= IUTF8;
  if (tcsetattr(pt_pair_[PT_SLAVE_FD], TCSANOW, &termios) != 0) {
    CloseFdPair(pt_pair);
    return false;
  }

  return true;
}

bool ProcessProxy::LaunchProcess(const base::CommandLine& cmdline,
                                 const std::string& user_id_hash,
                                 int slave_fd,
                                 std::string* id) {
  base::LaunchOptions options;

  // Redirect crosh  process' output and input so we can read it.
  options.fds_to_remap.push_back(std::make_pair(slave_fd, STDIN_FILENO));
  options.fds_to_remap.push_back(std::make_pair(slave_fd, STDOUT_FILENO));
  options.fds_to_remap.push_back(std::make_pair(slave_fd, STDERR_FILENO));
  // Do not set NO_NEW_PRIVS on processes if the system is in dev-mode. This
  // permits sudo in the crosh shell when in developer mode.
  options.allow_new_privs = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kSystemInDevMode);
  options.ctrl_terminal_fd = slave_fd;
  // TODO(vapier): Ideally we'd just use the env settings from hterm itself.
  // We can't let the user inject any env var they want, but we should be able
  // to filter the $TERM value dynamically.
  options.environment["TERM"] = "xterm-256color";
  options.environment["CROS_USER_ID_HASH"] = user_id_hash;

  // Launch the process.
  process_ = base::LaunchProcess(cmdline, options);

  // If the process is valid, generate a new random id.
  // https://crbug.com/352465
  if (process_.IsValid()) {
    // We use the GUID API as it's trivial and works well enough.
    // We prepend the pid to avoid random number collisions.  It should be a
    // guaranteed unique id for the life of this Chrome session.
    *id = base::NumberToString(process_.Pid()) + "-" +
          base::Uuid::GenerateRandomV4().AsLowercaseString();
  }

  // TODO(rvargas) crbug/417532: This is somewhat wrong but the interface of
  // Open vends pid_t* so ownership is quite vague anyway, and Process::Close
  // doesn't do much in POSIX.
  return process_.IsValid();
}

void ProcessProxy::CloseFdPair(int* pipe) {
  CloseFd(&(pipe[PT_MASTER_FD]));
  CloseFd(&(pipe[PT_SLAVE_FD]));
}

void ProcessProxy::CloseFd(int* fd) {
  if (*fd != base::kInvalidFd) {
    if (IGNORE_EINTR(close(*fd)) != 0)
      DPLOG(WARNING) << "close fd failed.";
  }
  *fd = base::kInvalidFd;
}

void ProcessProxy::ClearFdPair(int* pipe) {
  pipe[PT_MASTER_FD] = base::kInvalidFd;
  pipe[PT_SLAVE_FD] = base::kInvalidFd;
}

const base::Process* ProcessProxy::GetProcessForTesting() {
  return &process_;
}

}  // namespace chromeos
