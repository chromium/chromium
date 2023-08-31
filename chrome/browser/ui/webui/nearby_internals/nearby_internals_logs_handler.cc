// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_logs_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"

namespace {

// Keys in the JSON representation of a log message
const char kLogMessageTextKey[] = "text";
const char kLogMessageFeatureKey[] = "feature";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value::Dict LogMessageToDictionary(
    const CrossDeviceLogBuffer::LogMessage& log_message) {
  base::Value::Dict dictionary;
  dictionary.Set(kLogMessageTextKey, log_message.text);
  dictionary.Set(kLogMessageFeatureKey, int(log_message.feature));
  dictionary.Set(kLogMessageTimeKey,
                 base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary.Set(kLogMessageFileKey, log_message.file);
  dictionary.Set(kLogMessageLineKey, log_message.line);
  dictionary.Set(kLogMessageSeverityKey, log_message.severity);
  return dictionary;
}
}  // namespace

NearbyInternalsLogsHandler::NearbyInternalsLogsHandler() {}

NearbyInternalsLogsHandler::~NearbyInternalsLogsHandler() = default;

void NearbyInternalsLogsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&NearbyInternalsLogsHandler::HandleGetLogMessages,
                          base::Unretained(this)));
}

void NearbyInternalsLogsHandler::OnJavascriptAllowed() {
  observation_.Observe(CrossDeviceLogBuffer::GetInstance());
}

void NearbyInternalsLogsHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void NearbyInternalsLogsHandler::HandleGetLogMessages(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  base::Value::List list;
  for (const auto& log : *CrossDeviceLogBuffer::GetInstance()->logs()) {
    list.Append(LogMessageToDictionary(log));
  }
  ResolveJavascriptCallback(callback_id, list);
}

void NearbyInternalsLogsHandler::OnCrossDeviceLogBufferCleared() {
  FireWebUIListener("log-buffer-cleared");
}

void NearbyInternalsLogsHandler::OnLogMessageAdded(
    const CrossDeviceLogBuffer::LogMessage& log_message) {
  FireWebUIListener("log-message-added", LogMessageToDictionary(log_message));
}
