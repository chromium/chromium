// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/quick_pair/quick_pair_handler.h"

#include <memory>

#include "ash/quick_pair/ui/fast_pair/fast_pair_notification_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "ui/gfx/image/image.h"

namespace {
// Keys in the JSON representation of a log message
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Test device metadata for debug purposes
const char16_t kTestDeviceName[] = u"Pixel Buds";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value LogMessageToDictionary(
    const ash::quick_pair::LogBuffer::LogMessage& log_message) {
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

QuickPairHandler::QuickPairHandler()
    : fast_pair_notification_controller_(
          std::make_unique<ash::quick_pair::FastPairNotificationController>()) {
}

QuickPairHandler::~QuickPairHandler() = default;

void QuickPairHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getQuickPairLogMessages",
      base::BindRepeating(&QuickPairHandler::HandleGetLogMessages,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyFastPairError",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairError,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyFastPairDiscovery",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairDiscovery,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "notifyFastPairPairing",
      base::BindRepeating(&QuickPairHandler::NotifyFastPairPairing,
                          base::Unretained(this)));
}

void QuickPairHandler::OnJavascriptAllowed() {
  observation_.Observe(ash::quick_pair::LogBuffer::GetInstance());
}

void QuickPairHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void QuickPairHandler::HandleGetLogMessages(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];
  base::Value list(base::Value::Type::LIST);
  for (const auto& log : *ash::quick_pair::LogBuffer::GetInstance()->logs()) {
    list.Append(LogMessageToDictionary(log));
  }
  ResolveJavascriptCallback(callback_id, list);
}

void QuickPairHandler::OnLogBufferCleared() {
  FireWebUIListener("quick-pair-log-buffer-cleared");
}

void QuickPairHandler::OnLogMessageAdded(
    const ash::quick_pair::LogBuffer::LogMessage& log_message) {
  FireWebUIListener("quick-pair-log-message-added",
                    LogMessageToDictionary(log_message));
}

void QuickPairHandler::NotifyFastPairError(const base::ListValue* args) {
  fast_pair_notification_controller_->ShowErrorNotification(
      kTestDeviceName, gfx::Image(), base::DoNothing(), base::DoNothing());
}

void QuickPairHandler::NotifyFastPairDiscovery(const base::ListValue* args) {
  fast_pair_notification_controller_->ShowDiscoveryNotification(
      kTestDeviceName, gfx::Image(), base::DoNothing(), base::DoNothing());
}

void QuickPairHandler::NotifyFastPairPairing(const base::ListValue* args) {
  fast_pair_notification_controller_->ShowPairingNotification(
      kTestDeviceName, gfx::Image(), base::DoNothing(), base::DoNothing());
}
