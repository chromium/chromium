// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/devtools_events_logger.h"

#include "base/json/json_writer.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"

DevToolsEventsLogger::DevToolsEventsLogger(Log* log, const base::Value& prefs)
    : log_(log), prefs_(prefs) {}

inline DevToolsEventsLogger::~DevToolsEventsLogger() {}

Status DevToolsEventsLogger::OnConnected(DevToolsClient* client) {
  for (const auto& entry : prefs_->GetList())
    events_.insert(entry.is_string() ? entry.GetString() : std::string());
  return Status(kOk);
}

Status DevToolsEventsLogger::OnEvent(DevToolsClient* client,
                                     const std::string& method,
                                     const base::Value::Dict& params) {
  auto it = events_.find(method);
  if (it != events_.end()) {
    base::Value::Dict log_message_dict;
    log_message_dict.Set("method", method);
    log_message_dict.Set("params", params.Clone());
    std::string log_message_json;
    base::JSONWriter::Write(log_message_dict, &log_message_json);

    log_->AddEntry(Log::kInfo, log_message_json);
  }
  return Status(kOk);
}
