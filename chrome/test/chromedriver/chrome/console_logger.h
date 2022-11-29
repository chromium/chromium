// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CONSOLE_LOGGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CONSOLE_LOGGER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class Log;

// Translates DevTools Console.messageAdded events into Log messages.
//
// The message format, in terms of Console.ConsoleMessage fields, is:
// "<url or source> [<line>[:<column>]] text"
//
// Translates the level into Log::Level, drops all other fields.
class ConsoleLogger : public DevToolsEventListener {
 public:
  // Creates a ConsoleLogger that creates entries in the given Log object.
  // The log is owned elsewhere and must not be null.
  explicit ConsoleLogger(Log* log);

  ConsoleLogger(const ConsoleLogger&) = delete;
  ConsoleLogger& operator=(const ConsoleLogger&) = delete;

  // Enables Console events for the client, which must not be null.
  Status OnConnected(DevToolsClient* client) override;
  // Translates an event into a log entry.
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  raw_ptr<Log> log_;  // The log where to create entries.

  Status OnLogEntryAdded(const base::Value::Dict& params);
  Status OnRuntimeConsoleApiCalled(const base::Value::Dict& params);
  Status OnRuntimeExceptionThrown(const base::Value::Dict& params);
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CONSOLE_LOGGER_H_
