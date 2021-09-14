// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

ConsolidatedConsentScreenView::ScreenConfig::ScreenConfig() = default;

ConsolidatedConsentScreenView::ScreenConfig::~ScreenConfig() = default;

constexpr StaticOobeScreenId ConsolidatedConsentScreenView::kScreenId;

ConsolidatedConsentScreenHandler::ConsolidatedConsentScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.ConsolidatedConsentScreen.userActed");
}

ConsolidatedConsentScreenHandler::~ConsolidatedConsentScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ConsolidatedConsentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void ConsolidatedConsentScreenHandler::Initialize() {}

void ConsolidatedConsentScreenHandler::Show(const ScreenConfig& config) {}

void ConsolidatedConsentScreenHandler::Bind(ConsolidatedConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ConsolidatedConsentScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ConsolidatedConsentScreenHandler::RegisterMessages() {}

void ConsolidatedConsentScreenHandler::HandleAccept(
    bool enable_stats_usage,
    bool enable_backup_restore,
    bool enable_location_services,
    const std::string& tos_content) {
  screen_->OnAccept(enable_stats_usage, enable_backup_restore,
                    enable_location_services, tos_content);
}

void ConsolidatedConsentScreenHandler::SetUsageMode(bool enabled,
                                                    bool managed) {
  // TODO(crbug.com/1201616): CallJS to updated the UI
}

void ConsolidatedConsentScreenHandler::SetBackupMode(bool enabled,
                                                     bool managed) {
  // TODO(crbug.com/1201616): CallJS to update the UI
}

void ConsolidatedConsentScreenHandler::SetLocationMode(bool enabled,
                                                       bool managed) {
  // TODO(crbug.com/1201616): CallJS to update the UI
}

}  // namespace chromeos
