// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

namespace {

// These values must be kept in sync with UIState in JS code.
constexpr const char kCheckingForUpdate[] = "checking";
constexpr const char kUpdateInProgress[] = "update";
constexpr const char kRestartInProgress[] = "restart";
constexpr const char kManualReboot[] = "reboot";
constexpr const char kCellularPermission[] = "cellular";

}  // namespace

ConsumerUpdateScreenHandler::ConsumerUpdateScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ConsumerUpdateScreenHandler::~ConsumerUpdateScreenHandler() = default;

void ConsumerUpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("consumerUpdateScreenAcceptButton",
               IDS_CONSUMER_UPDATE_ACCEPT_BUTTON);
  builder->Add("consumerUpdateScreenSkipButton",
               IDS_CONSUMER_UPDATE_SKIP_BUTTON);
  builder->Add("consumerUpdateScreenCellularTitle",
               IDS_CONSUMER_UPDATE_CELLULAR_TITLE);
  builder->Add("consumerUpdateScreenInProgressTitle",
               IDS_CONSUMER_UPDATE_PROGRESS_TITLE);
  builder->Add("consumerUpdateScreenInProgressSubtitle",
               IDS_CONSUMER_UPDATE_PROGRESS_SUBTITLE);
  builder->Add("consumerUpdateScreenInProgressAdditionalSubtitle",
               IDS_CONSUMER_UPDATE_PROGRESS_ADDITIONAL_SUBTITLE);
}

void ConsumerUpdateScreenHandler::Show() {
  ShowInWebUI();
}

void ConsumerUpdateScreenHandler::SetUpdateState(
    ConsumerUpdateScreenView::UIState value) {
  switch (value) {
    case ConsumerUpdateScreenView::UIState::kCheckingForUpdate:
      CallExternalAPI("setUpdateState", kCheckingForUpdate);
      break;
    case ConsumerUpdateScreenView::UIState::kUpdateInProgress:
      CallExternalAPI("setUpdateState", kUpdateInProgress);
      break;
    case ConsumerUpdateScreenView::UIState::kRestartInProgress:
      CallExternalAPI("setUpdateState", kRestartInProgress);
      break;
    case ConsumerUpdateScreenView::UIState::kManualReboot:
      CallExternalAPI("setUpdateState", kManualReboot);
      break;
    case ConsumerUpdateScreenView::UIState::kCellularPermission:
      CallExternalAPI("setUpdateState", kCellularPermission);
      break;
  }
}

void ConsumerUpdateScreenHandler::SetUpdateStatus(
    int percent,
    const std::u16string& percent_message,
    const std::u16string& timeleft_message) {
  CallExternalAPI("setUpdateStatus", percent, percent_message,
                  timeleft_message);
}

void ConsumerUpdateScreenHandler::ShowLowBatteryWarningMessage(bool value) {
  CallExternalAPI("showLowBatteryWarningMessage", value);
}

void ConsumerUpdateScreenHandler::SetAutoTransition(bool value) {
  CallExternalAPI("setAutoTransition", value);
}

void ConsumerUpdateScreenHandler::SetIsUpdateMandatory(bool value) {
  CallExternalAPI("setIsUpdateMandatory", value);
}

}  // namespace ash
