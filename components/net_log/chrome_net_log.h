// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
#define COMPONENTS_NET_LOG_CHROME_NET_LOG_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "net/log/net_log.h"

namespace base {
class FilePath;
class Value;
}

namespace net {
class FileNetLogObserver;
}

namespace net_log {

// ChromeNetLog is an implementation of NetLog that manages common observers
// (for --log-net-log, chrome://net-export/, tracing), as well as acting as the
// entry point for other consumers.
//
// Threading:
//   * The methods on net::NetLog are threadsafe
//   * The methods defined by ChromeNetLog must be sequenced.
class ChromeNetLog : public net::NetLog {
 public:
  ChromeNetLog();
  ~ChromeNetLog() override;

  // Starts streaming the NetLog events to a file on disk. This will continue
  // until the application shuts down.
  // * |path| - destination file path of the log file.
  // * |capture_mode| - capture mode for event granularity.
  void StartWritingToFile(
      const base::FilePath& path,
      net::NetLogCaptureMode capture_mode,
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

  // Returns a Value containing constants needed to load a log file.
  // Safe to call on any thread.
  static std::unique_ptr<base::Value> GetConstants(
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

  // Returns only platform-specific constants. This doesn't include the net/
  // baseline, only Chrome-specific platform information.
  static std::unique_ptr<base::DictionaryValue> GetPlatformConstants(
      const base::CommandLine::StringType& command_line_string,
      const std::string& channel_string);

  // Notify the ChromeNetLog that things are shutting-down.
  //
  // If ChromeNetLog does not outlive the TaskScheduler, there is no need to
  // call this.
  //
  // However, if it can outlive the TaskScheduler, this should be called
  // before the TaskScheduler is shutdown. This allows for any file writers
  // using BLOCK_SHUTDOWN to finish posting their writes.
  //
  // Not calling this is not a fatal error, however may result in an incomplete
  // NetLog file being written to disk.
  void ShutDownBeforeTaskScheduler();

 private:
  // Deletes file_net_log_observer_.
  void ClearFileNetLogObserver();

  // This observer handles writing NetLogs specified via StartWritingToFile()
  // (In Chrome this corresponds to the --log-net-log command line).
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_CHROME_NET_LOG_H_
