// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"

#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

CategoriesSelectionScreenHandler::CategoriesSelectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

CategoriesSelectionScreenHandler::~CategoriesSelectionScreenHandler() = default;

void CategoriesSelectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("categoriesLoading", IDS_LOGIN_CATEGORIES_SCREEN_LOADING);
  builder->AddF("categoriesScreenTitle", IDS_LOGIN_CATEGORIES_SCREEN_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("categoriesScreenDescription",
               IDS_LOGIN_CATEGORIES_SCREEN_SUBTITLE);
  builder->Add("categoriesScreenSkip", IDS_LOGIN_CATEGORIES_SCREEN_SKIP);
}

void CategoriesSelectionScreenHandler::Show() {
  ShowInWebUI();
}

void CategoriesSelectionScreenHandler::SetCategoriesData(
    base::Value::Dict categories) {
  CallExternalAPI("setCategoriesData", std::move(categories));
}

void CategoriesSelectionScreenHandler::SetOverviewStep() {
  CallExternalAPI("setOverviewStep");
}

base::WeakPtr<CategoriesSelectionScreenView>
CategoriesSelectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
