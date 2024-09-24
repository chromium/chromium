// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

AccountSelectionScreenHandler::AccountSelectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

AccountSelectionScreenHandler::~AccountSelectionScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void AccountSelectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void AccountSelectionScreenHandler::Show() {
  ShowInWebUI();
}

void AccountSelectionScreenHandler::ShowStepProgress() {
  CallExternalAPI("showStepProgress");
}

void AccountSelectionScreenHandler::SetUserEmail(const std::string& email) {
  CallExternalAPI("setUserEmail", email);
}

base::WeakPtr<AccountSelectionScreenView>
AccountSelectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
