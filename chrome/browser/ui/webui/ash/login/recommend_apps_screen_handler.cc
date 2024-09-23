// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

enum class RecommendAppsScreenState {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should be kept in sync with
  // RecommendAppsScreenState in enums.xml.
  SHOW = 0,
  NO_SHOW = 1,
  ERROR = 2,

  kMaxValue = ERROR
};

void RecordUmaScreenState(RecommendAppsScreenState state) {
  base::UmaHistogramEnumeration("OOBE.RecommendApps.Screen.State", state);
}

}  // namespace

namespace ash {

RecommendAppsScreenHandler::RecommendAppsScreenHandler()
    : BaseScreenHandler(kScreenId) {}

RecommendAppsScreenHandler::~RecommendAppsScreenHandler() = default;

void RecommendAppsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("recommendAppsLoading", IDS_LOGIN_RECOMMEND_APPS_SCREEN_LOADING);
  builder->AddF("recommendAppsScreenTitle",
                IDS_LOGIN_RECOMMEND_APPS_SCREEN_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("recommendAppsScreenDescription",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION);
  builder->Add("recommendAppsSkip", IDS_LOGIN_RECOMMEND_APPS_SKIP);
  builder->Add("recommendAppsInstall", IDS_LOGIN_RECOMMEND_APPS_INSTALL);
  builder->Add("recommendAppsSelectAll", IDS_LOGIN_RECOMMEND_APPS_SELECT_ALL);
  builder->Add("recommendAppsInAppPurchases",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_IN_APP_PURCHASES);
  builder->Add("recommendAppsWasInstalled",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_WAS_INSTALLED);
  builder->Add("recommendAppsContainsAds",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_CONTAINS_ADS);
  builder->Add("recommendAppsDescriptionExpand",
               IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION_EXPAND_BUTTON);
}

void RecommendAppsScreenHandler::Show() {
  ShowInWebUI();
}

void RecommendAppsScreenHandler::OnLoadSuccess(base::Value app_list) {
  LoadAppListInUI(std::move(app_list));
}

void RecommendAppsScreenHandler::OnParseResponseError() {
  RecordUmaScreenState(RecommendAppsScreenState::NO_SHOW);
}

void RecommendAppsScreenHandler::LoadAppListInUI(base::Value app_list) {
  RecordUmaScreenState(RecommendAppsScreenState::SHOW);
  CallExternalAPI("loadAppList", std::move(app_list));
}

base::WeakPtr<RecommendAppsScreenView> RecommendAppsScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
