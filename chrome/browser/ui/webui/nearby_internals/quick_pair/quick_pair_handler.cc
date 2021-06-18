// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/quick_pair/quick_pair_handler.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "chromeos/components/quick_pair/common/logging.h"

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
    const chromeos::quick_pair::LogBuffer::LogMessage& log_message) {
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

QuickPairHandler::QuickPairHandler() {}

QuickPairHandler::~QuickPairHandler() = default;

void QuickPairHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getQuickPairLogMessages",
      base::BindRepeating(&QuickPairHandler::HandleGetLogMessages,
                          base::Unretained(this)));
}

void QuickPairHandler::OnJavascriptAllowed() {
  observation_.Observe(chromeos::quick_pair::LogBuffer::GetInstance());
}

void QuickPairHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void QuickPairHandler::HandleGetLogMessages(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  base::Value list(base::Value::Type::LIST);
  for (const auto& log :
       *chromeos::quick_pair::LogBuffer::GetInstance()->logs()) {
    list.Append(LogMessageToDictionary(log));
  }
  ResolveJavascriptCallback(callback_id, list);
}

void QuickPairHandler::OnLogBufferCleared() {
  FireWebUIListener("quick-pair-log-buffer-cleared");
}

void QuickPairHandler::OnLogMessageAdded(
    const chromeos::quick_pair::LogBuffer::LogMessage& log_message) {
  FireWebUIListener("quick-pair-log-message-added",
                    LogMessageToDictionary(log_message));
}
