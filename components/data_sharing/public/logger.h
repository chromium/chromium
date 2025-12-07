// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_H_

#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/data_sharing/public/logger_common.mojom.h"

namespace data_sharing {

// Helper class for aggregating logs about Data Sharing and potentially exposing
// them to a helper internals page.  Note that this class will not necessarily
// log or be stored unless there is a registered observer or
// --data-sharing-debug-logs is set.
class Logger {
 public:
  // A helper class to store state about a particular log.
  struct Entry {
   public:
    Entry(base::Time event_time,
          logger_common::mojom::LogSource log_source,
          const std::string& source_file,
          int source_line,
          const std::string& message);

    bool operator==(const Entry& other) const;

    base::Time event_time;
    logger_common::mojom::LogSource log_source;
    std::string source_file;
    int source_line;
    std::string message;
  };

  // Observer to be notified when new logs are added.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a new log is available (or when a new Observer is registered,
    // with any queued up logs.
    virtual void OnNewLog(const Entry& entry) = 0;
  };

  virtual ~Logger() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Whether or not logs are enabled.
  virtual bool ShouldEnableDebugLogs() = 0;

  // Saves a message log related to Data Sharing along with associated metadata.
  virtual void Log(base::Time event_time,
                   logger_common::mojom::LogSource log_source,
                   const std::string& source_file,
                   int source_line,
                   const std::string& message) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_LOGGER_H_
