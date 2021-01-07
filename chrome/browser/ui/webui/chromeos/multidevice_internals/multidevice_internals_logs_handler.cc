// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_internals/multidevice_internals_logs_handler.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace multidevice {

namespace {

// Keys in the JSON representation of a log message
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value LogMessageToDictionary(
    const chromeos::multidevice::LogBuffer::LogMessage& log_message) {
  base::Value dictionary(base::Value::Type::DICTIONARY);
  dictionary.SetStringKey(kLogMessageTextKey, log_message.text);
  dictionary.SetStringKey(
      kLogMessageTimeKey,
      base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary.SetStringKey(kLogMessageFileKey, log_message.file);
  dictionary.SetIntKey(kLogMessageLineKey, log_message.line);
  dictionary.SetIntKey(kLogMessageSeverityKey, log_message.severity);
  return dictionary;
}

}  // namespace

MultideviceLogsHandler::MultideviceLogsHandler() {}

MultideviceLogsHandler::~MultideviceLogsHandler() = default;

void MultideviceLogsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getMultideviceLogMessages",
      base::BindRepeating(&MultideviceLogsHandler::HandleGetLogMessages,
                          base::Unretained(this)));
}

void MultideviceLogsHandler::OnJavascriptAllowed() {
  observation_.Observe(multidevice::LogBuffer::GetInstance());
}

void MultideviceLogsHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void MultideviceLogsHandler::HandleGetLogMessages(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  base::Value list(base::Value::Type::LIST);
  for (const auto& log :
       *chromeos::multidevice::LogBuffer::GetInstance()->logs()) {
    list.Append(LogMessageToDictionary(log));
  }
  ResolveJavascriptCallback(callback_id, list);
}

void MultideviceLogsHandler::OnLogBufferCleared() {
  FireWebUIListener("multidevice-log-buffer-cleared");
}

void MultideviceLogsHandler::OnLogMessageAdded(
    const chromeos::multidevice::LogBuffer::LogMessage& log_message) {
  FireWebUIListener("multidevice-log-message-added",
                    LogMessageToDictionary(log_message));
}

}  // namespace multidevice

}  // namespace chromeos
