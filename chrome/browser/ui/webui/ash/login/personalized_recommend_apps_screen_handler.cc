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

namespace ash {

PersonalizedRecommendAppsScreenHandler::PersonalizedRecommendAppsScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PersonalizedRecommendAppsScreenHandler::
    ~PersonalizedRecommendAppsScreenHandler() = default;

void PersonalizedRecommendAppsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void PersonalizedRecommendAppsScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<PersonalizedRecommendAppsScreenView>
PersonalizedRecommendAppsScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
