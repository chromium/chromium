// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_LOGGING_H_
#define CHROME_TEST_CHROMEDRIVER_LOGGING_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"

struct Capabilities;
class CommandListener;
class DevToolsEventListener;
class ListValue;
struct Session;
class Status;

namespace internal {
static const size_t kMaxReturnedEntries = 100000;
}  // namespace internal

// Accumulates WebDriver Logging API entries of a given type and minimum level.
// See https://code.google.com/p/selenium/wiki/Logging.
class WebDriverLog : public Log {
 public:
  static const char kBrowserType[];
  static const char kDriverType[];
  static const char kPerformanceType[];
  static const char kDevToolsType[];

  // Converts WD wire protocol level name -> Level, false on bad name.
  static bool NameToLevel(const std::string& name, Level* out_level);

  // Creates a WebDriverLog with the given type and minimum level.
  WebDriverLog(const std::string& type, Level min_level);
  ~WebDriverLog() override;

  // Returns entries accumulated so far, as a ListValue ready for serialization
  // into the wire protocol response to the "/log" command.
  // The caller assumes ownership of the ListValue, and the WebDriverLog
  // creates and owns a new empty ListValue for further accumulation.
  std::unique_ptr<base::ListValue> GetAndClearEntries();

  // Finds the first error message in the log and returns it. If none exist,
  // returns an empty string. Does not clear entries.
  std::string GetFirstErrorMessage() const;

  // Translates a Log entry level into a WebDriver level and stores the entry.
  void AddEntryTimestamped(const base::Time& timestamp,
                           Level level,
                           const std::string& source,
                           const std::string& message) override;

  // Whether or not batches_of_entries_ is empty when it is being emptied.
  bool Emptied() const override;

  const std::string& type() const;
  void set_min_level(Level min_level);
  Level min_level() const;

 private:
  const std::string type_;  // WebDriver log type.
  Level min_level_;  // Minimum level of entries to store.
  // Log is empty when it is emptied, or when it is initialized (because we
  // want GetLog to collect trace events initially).
  bool emptied_;

  // A queue of batches of entries. Each batch can have no more than
  // |kMaxReturnedEntries| values in it. This is to avoid HTTP response buffer
  // overflow (crbug.com/681892).
  base::circular_deque<std::unique_ptr<base::ListValue>> batches_of_entries_;

  DISALLOW_COPY_AND_ASSIGN(WebDriverLog);
};

// Initializes logging system for ChromeDriver. Returns true on success.
bool InitLogging(uint16_t port);

// Creates |Log|s, |DevToolsEventListener|s, and |CommandListener|s based on
// logging preferences.
Status CreateLogs(
    const Capabilities& capabilities,
    const Session* session,
    std::vector<std::unique_ptr<WebDriverLog>>* out_logs,
    std::vector<std::unique_ptr<DevToolsEventListener>>* out_devtools_listeners,
    std::vector<std::unique_ptr<CommandListener>>* out_command_listeners);

const char* GetPortProtectionMessage();

#endif  // CHROME_TEST_CHROMEDRIVER_LOGGING_H_
