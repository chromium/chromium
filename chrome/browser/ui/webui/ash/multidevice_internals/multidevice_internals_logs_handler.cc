// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/multidevice_internals/multidevice_internals_logs_handler.h"

#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/log_buffer.h"

namespace ash::multidevice {

namespace {

// Keys in the JSON representation of a log message
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value::Dict LogMessageToDictionary(
    const LogBuffer::LogMessage& log_message) {
  return base::Value::Dict()
      .Set(kLogMessageTextKey, log_message.text)
      .Set(kLogMessageTimeKey,
           base::TimeFormatTimeOfDayWithMilliseconds(log_message.time))
      .Set(kLogMessageFileKey, log_message.file)
      .Set(kLogMessageLineKey, log_message.line)
      .Set(kLogMessageSeverityKey, log_message.severity);
}

}  // namespace

MultideviceLogsHandler::MultideviceLogsHandler() = default;

MultideviceLogsHandler::~MultideviceLogsHandler() = default;

void MultideviceLogsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getMultideviceLogMessages",
      base::BindRepeating(&MultideviceLogsHandler::HandleGetLogMessages,
                          base::Unretained(this)));
}

void MultideviceLogsHandler::OnJavascriptAllowed() {
  observation_.Observe(LogBuffer::GetInstance());
}

void MultideviceLogsHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void MultideviceLogsHandler::HandleGetLogMessages(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  base::Value::List list;
  for (const auto& log : *LogBuffer::GetInstance()->logs()) {
    list.Append(LogMessageToDictionary(log));
  }
  ResolveJavascriptCallback(callback_id, list);
}

void MultideviceLogsHandler::OnLogBufferCleared() {
  FireWebUIListener("multidevice-log-buffer-cleared");
}

void MultideviceLogsHandler::OnLogMessageAdded(
    const LogBuffer::LogMessage& log_message) {
  FireWebUIListener("multidevice-log-message-added",
                    LogMessageToDictionary(log_message));
}

}  // namespace ash::multidevice
