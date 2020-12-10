// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_page_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "chromeos/components/help_app_ui/help_app_ui.h"
#include "chromeos/components/help_app_ui/help_app_ui_delegate.h"
#include "chromeos/constants/chromeos_features.h"

HelpAppPageHandler::HelpAppPageHandler(
    chromeos::HelpAppUI* help_app_ui,
    mojo::PendingReceiver<help_app_ui::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)),
      help_app_ui_(help_app_ui),
      is_lss_enabled_(
          base::FeatureList::IsEnabled(
              chromeos::features::kHelpAppSearchServiceIntegration) &&
          base::FeatureList::IsEnabled(
              chromeos::features::kEnableLocalSearchService)) {}

HelpAppPageHandler::~HelpAppPageHandler() = default;

void HelpAppPageHandler::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  auto error_message = help_app_ui_->delegate()->OpenFeedbackDialog();
  std::move(callback).Run(std::move(error_message));
}

void HelpAppPageHandler::ShowParentalControls() {
  help_app_ui_->delegate()->ShowParentalControls();
}

void HelpAppPageHandler::IsLssEnabled(IsLssEnabledCallback callback) {
  std::move(callback).Run(is_lss_enabled_);
}
