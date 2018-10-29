// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_events_logger.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

DevToolsEventsLogger::DevToolsEventsLogger(Log* log,
                                           const base::ListValue* prefs)
    : log_(log),
      prefs_(prefs) {}

inline DevToolsEventsLogger::~DevToolsEventsLogger() {}

Status DevToolsEventsLogger::OnConnected(DevToolsClient* client) {
  for (auto it = prefs_->begin(); it != prefs_->end(); ++it) {
    std::string event;
    it->GetAsString(&event);
    events_.insert(event);
  }
  return Status(kOk);
}

Status DevToolsEventsLogger::OnEvent(DevToolsClient* client,
                                     const std::string& method,
                                     const base::DictionaryValue& params) {
  auto it = events_.find(method);
  if (it != events_.end()) {
    base::DictionaryValue log_message_dict;
    log_message_dict.SetString("method", method);
    log_message_dict.SetKey("params", params.Clone());
    std::string log_message_json;
    base::JSONWriter::Write(log_message_dict, &log_message_json);

    log_->AddEntry(Log::kInfo, log_message_json);
  }
  return Status(kOk);
}
