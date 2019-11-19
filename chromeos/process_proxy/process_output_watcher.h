// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_
#define CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

enum ProcessOutputType {
  PROCESS_OUTPUT_TYPE_OUT,
  PROCESS_OUTPUT_TYPE_EXIT
};

typedef base::Callback<void(ProcessOutputType,
                            const std::string&,
                            const base::Closure&)> ProcessOutputCallback;

// Observes output on |out_fd| and invokes |callback| when some output is
// detected. It assumes UTF8 output.
class CHROMEOS_EXPORT ProcessOutputWatcher {
 public:
  ProcessOutputWatcher(int out_fd, const ProcessOutputCallback& callback);
  ~ProcessOutputWatcher();

  void Start();

 private:
  // Called when |process_output_file_| is readable without blocking.
  void OnProcessOutputCanReadWithoutBlocking();

  // Listens to output from fd passed to the constructor.
  void WatchProcessOutput();

  // Reads data from fd and invokes callback |on_read_callback_| with read data.
  void ReadFromFd(int fd);

  // Checks if the read buffer has any trailing incomplete UTF8 characters and
  // returns the read buffer size without them.
  size_t OutputSizeWithoutIncompleteUTF8();

  // Processes new |read_buffer_| state and notifies observer about new process
  // output.
  void ReportOutput(ProcessOutputType type,
                    size_t new_bytes_count,
                    const base::Closure& callback);

  char read_buffer_[4096];
  // Maximum read buffer content size.
  size_t read_buffer_capacity_;
  // Current read bufferi content size.
  size_t read_buffer_size_;

  // Contains file descsriptor to which watched process output is written.
  base::File process_output_file_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> output_file_watcher_;

  // Callback that will be invoked when some output is detected.
  ProcessOutputCallback on_read_callback_;

  base::WeakPtrFactory<ProcessOutputWatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProcessOutputWatcher);
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_PROXY_PROCESS_OUTPUT_WATCHER_H_
