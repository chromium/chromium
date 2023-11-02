// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
#define CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_

#include <fcntl.h>
#include <signal.h>

#include <cstdio>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "chromeos/process_proxy/process_output_watcher.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace chromeos {
class ProcessOutputWatcher;
}  // namespace chromeos

namespace chromeos {

// Proxy to a single ChromeOS process.
// This is refcounted. Note that output watcher, when it gets triggered owns a
// a callback with ref to this, so in order for this to be freed, the watcher
// must be destroyed. This is done in Close.
class ProcessProxy : public base::RefCountedThreadSafe<ProcessProxy> {
 public:
  using OutputCallback =
      base::RepeatingCallback<void(ProcessOutputType output_type,
                                   const std::string& output_data)>;

  ProcessProxy();

  ProcessProxy(const ProcessProxy&) = delete;
  ProcessProxy& operator=(const ProcessProxy&) = delete;

  // Opens a process using command |command| for the user with hash
  // |user_id_hash|.  Returns process ID on success, -1 on failure.
  bool Open(const base::CommandLine& cmdline,
            const std::string& user_id_hash,
            std::string* id);

  bool StartWatchingOutput(
      const scoped_refptr<base::SingleThreadTaskRunner>& watcher_runner,
      const scoped_refptr<base::SequencedTaskRunner>& callback_runner,
      const OutputCallback& callback);

  // Sends some data to the process.
  void Write(const std::string& text, base::OnceCallback<void(bool)> callback);

  // Closes the process.
  void Close();

  // Notifies underlaying process of terminal size change.
  bool OnTerminalResize(int width, int height);

  // Should be called when process output that was reported via |callback_| gets
  // handled. It runs, and then resets |output_ack_callback_|.
  void AckOutput();

  // Get the process handle for testing purposes.
  const base::Process* GetProcessForTesting();

 private:
  friend class base::RefCountedThreadSafe<ProcessProxy>;
  // We want this be used as ref counted object only.
  ~ProcessProxy();

  // Create master and slave end of pseudo terminal that will be used to
  // communicate with process.
  // pt_pair[0] -> master, pt_pair[1] -> slave.
  // pt_pair must be allocated (to size at least 2).
  bool CreatePseudoTerminalPair(int *pt_pair);

  // Launches command in a new terminal process, mapping its stdout and stdin to
  // |slave_fd|.
  // Returns launched process id, or -1 on failure.
  bool LaunchProcess(const base::CommandLine& cmdline,
                     const std::string& user_id_hash,
                     int slave_fd,
                     std::string* id);

  // Gets called by output watcher when the process writes something to its
  // output streams. If set, |callback| should be called when the output is
  // handled.
  void OnProcessOutput(ProcessOutputType type, const std::string& output);
  void CallOnProcessOutputCallback(ProcessOutputType type,
                                   const std::string& output);

  void StopWatching();

  // Expects array of 2 file descripters.
  void CloseFdPair(int* pipe);
  // Expects pointer to single file descriptor.
  void CloseFd(int* fd);
  // Expects array of 2 file descripters.
  void ClearFdPair(int* pipe);

  bool process_launched_;

  bool callback_set_;
  // Callback used to report process output detected by proces output watcher.
  OutputCallback callback_;
  scoped_refptr<base::TaskRunner> callback_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> watcher_runner_;

  std::unique_ptr<ProcessOutputWatcher> output_watcher_;

  base::Process process_;

  int pt_pair_[2];
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_H_
