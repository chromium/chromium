// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_EVENT_LOGGER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_EVENT_LOGGER_H_

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"

namespace offline_pages {

// Maximum number of recorded Logs to keep track of at any moment.
constexpr size_t kMaxLogCount = 50;

// Facilitates the logging of events. Subclasses should create methods that
// call RecordActivity to write into the log. |SetIsLogging|, |GetLogs|, and
// |Clear| are called from the chrome://offline-internals page.
//
// Logging should be done by calling the corresponding subclass methods when
// a loggable event occurs (i.e. when status has changed for a save request
// or when an offlined page has been accessed/saved).
//
// This log only keeps track of the last |kMaxLogCount| events.
class OfflineEventLogger {
 public:
  // This client interface should be implemented by the class which provides the
  // ability to pipe the log somewhere else (Eg. a java class which can write
  // logs into a file). It's optional and uses SetClient() to attach the client
  // to the event logger instance.
  class Client {
   public:
    virtual ~Client() = default;
    virtual void CustomLog(const std::string& message) = 0;
  };

  OfflineEventLogger();

  ~OfflineEventLogger();

  // Clears the recorded activities.
  void Clear();

  // Turns logging on/off.
  void SetIsLogging(bool is_logging);

  // Returns whether we are currently logging.
  bool GetIsLogging();

  // Dumps the current activity list into |records|.
  void GetLogs(std::vector<std::string>* records);

  // Write the activity into the cycling log if we are currently logging.
  void RecordActivity(const std::string& activity);

  // Sets the client for custom logging process if needed.
  void SetClient(Client* client);

 private:
  // Recorded offline page activities.
  base::circular_deque<std::string> activities_;

  // Whether we are currently recording logs or not.
  bool is_logging_;

  // Not owned.
  raw_ptr<Client> client_;
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_EVENT_LOGGER_H_
