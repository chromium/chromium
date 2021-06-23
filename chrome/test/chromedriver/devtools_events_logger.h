// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENTS_LOGGER_H_
#define CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENTS_LOGGER_H_

#include <string>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

// Collects DevTools events into Log messages with info level.
//
// The message is a JSON string of the following structure:
// {
//    "message": { "method": "...", "params": { ... }}  // DevTools message.
// }

class DevToolsEventsLogger : public DevToolsEventListener {
 public:
  // Creates a |DevToolsEventsLogger| with specific preferences.
  DevToolsEventsLogger(Log* log, const base::ListValue* prefs);

  ~DevToolsEventsLogger() override;

  Status OnConnected(DevToolsClient* client) override;

  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::DictionaryValue& params) override;

 private:
  Log* log_;  // The log where to create entries.

  const base::ListValue* prefs_;
  std::unordered_set<std::string> events_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEventsLogger);
};

#endif  // CHROME_TEST_CHROMEDRIVER_DEVTOOLS_EVENTS_LOGGER_H_
