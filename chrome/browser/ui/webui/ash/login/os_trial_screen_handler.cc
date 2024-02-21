// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/os_trial_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

OsTrialScreenHandler::OsTrialScreenHandler() : BaseScreenHandler(kScreenId) {}

OsTrialScreenHandler::~OsTrialScreenHandler() = default;

void OsTrialScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("osTrialTitle", IDS_OS_TRIAL_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialSubtitle", IDS_OS_TRIAL_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialInstallTitle", IDS_OS_TRIAL_INSTALL_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialInstallSubtitle", IDS_OS_TRIAL_INSTALL_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osTrialTryTitle", IDS_OS_TRIAL_TRY_TITLE);
  builder->AddF("osTrialTrySubtitle", IDS_OS_TRIAL_TRY_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osTrialNextButton", IDS_OS_TRIAL_NEXT_BUTTON);
}

void OsTrialScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<OsTrialScreenView> OsTrialScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
