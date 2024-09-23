// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

PersonalizedRecommendAppsScreenHandler::PersonalizedRecommendAppsScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PersonalizedRecommendAppsScreenHandler::
    ~PersonalizedRecommendAppsScreenHandler() = default;

void PersonalizedRecommendAppsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("personalizedRecommendedLoading",
               IDS_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_SCREEN_LOADING);
  builder->AddF("personalizedRecommendedAppsScreenTitle",
                IDS_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_SCREEN_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("personalizedRecommendedAppsScreenDescription",
               IDS_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_SCREEN_SUBTITLE);
  builder->Add("personalizedRecommendedAppsScreenSkip",
               IDS_LOGIN_PERSONALIZED_RECOMMEND_APPS_SCREEN_SCREEN_SKIP);
}

void PersonalizedRecommendAppsScreenHandler::Show() {
  ShowInWebUI();
}

void PersonalizedRecommendAppsScreenHandler::SetAppsAndUseCasesData(
    base::Value::List useCasesApps) {
  CallExternalAPI("setAppsAndUseCasesData", std::move(useCasesApps));
}

void PersonalizedRecommendAppsScreenHandler::SetOverviewStep() {
  CallExternalAPI("setOverviewStep");
}

base::WeakPtr<PersonalizedRecommendAppsScreenView>
PersonalizedRecommendAppsScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
