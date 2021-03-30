// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

constexpr const char kUserActionSkip[] = "recommendAppsSkip";
constexpr const char kUserActionInstall[] = "recommendAppsInstall";

constexpr const int kMaxAppCount = 21;

enum class RecommendAppsScreenState {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should be kept in sync with
  // RecommendAppsScreenState in enums.xml.
  SHOW = 0,
  NO_SHOW = 1,
  ERROR = 2,

  kMaxValue = ERROR
};

enum class RecommendAppsScreenAction {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should be kept in sync with
  // RecommendAppsScreenAction in enums.xml.
  SKIPPED = 0,
  RETRIED = 1,
  SELECTED_NONE = 2,
  APP_SELECTED = 3,

  kMaxValue = APP_SELECTED
};

void RecordUmaUserSelectionAppCount(int app_count) {
  UMA_HISTOGRAM_EXACT_LINEAR("OOBE.RecommendApps.Screen.SelectedAppCount",
                             app_count, kMaxAppCount);
}

void RecordUmaSelectedRecommendedPercentage(
    int selected_recommended_percentage) {
  UMA_HISTOGRAM_PERCENTAGE(
      "OOBE.RecommendApps.Screen.SelectedRecommendedPercentage",
      selected_recommended_percentage);
}

void RecordUmaScreenState(RecommendAppsScreenState state) {
  UMA_HISTOGRAM_ENUMERATION("OOBE.RecommendApps.Screen.State", state);
}

void RecordUmaScreenAction(RecommendAppsScreenAction action) {
  UMA_HISTOGRAM_ENUMERATION("OOBE.RecommendApps.Screen.Action", action);
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId RecommendAppsScreenView::kScreenId;

RecommendAppsScreenHandler::RecommendAppsScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
}

RecommendAppsScreenHandler::~RecommendAppsScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void RecommendAppsScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  if (features::IsNewOobeLayoutEnabled()) {
    builder->Add("recommendAppsScreenTitle",
                 IDS_LOGIN_RECOMMEND_APPS_SCREEN_TITLE_NEW);
    builder->Add("recommendAppsScreenDescription",
                 IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION_NEW);
    builder->Add("recommendAppsSkip", IDS_LOGIN_RECOMMEND_APPS_DO_IT_LATER);
    builder->Add("recommendAppsInstall", IDS_LOGIN_RECOMMEND_APPS_DONE);
  } else {
    builder->Add("recommendAppsScreenTitle",
                 IDS_LOGIN_RECOMMEND_APPS_SCREEN_TITLE);
    builder->Add("recommendAppsScreenDescription",
                 IDS_LOGIN_RECOMMEND_APPS_SCREEN_DESCRIPTION);
    builder->Add("recommendAppsSkip", IDS_LOGIN_RECOMMEND_APPS_SKIP);
    builder->Add("recommendAppsInstall", IDS_LOGIN_RECOMMEND_APPS_INSTALL);
  }
  builder->Add("recommendAppsLoading", IDS_LOGIN_RECOMMEND_APPS_SCREEN_LOADING);
  builder->Add("recommendAppsSelectAll", IDS_LOGIN_RECOMMEND_APPS_SELECT_ALL);
}

void RecommendAppsScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback(kUserActionSkip, &RecommendAppsScreenHandler::OnUserSkip);
  AddRawCallback(kUserActionInstall,
                 &RecommendAppsScreenHandler::HandleInstall);
}

void RecommendAppsScreenHandler::Bind(RecommendAppsScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void RecommendAppsScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  pref_service_ = profile->GetPrefs();
}

void RecommendAppsScreenHandler::Hide() {}

void RecommendAppsScreenHandler::OnLoadSuccess(const base::Value& app_list) {
  recommended_app_count_ = static_cast<int>(app_list.GetList().size());
  LoadAppListInUI(app_list);
}

void RecommendAppsScreenHandler::OnParseResponseError() {
  RecordUmaScreenState(RecommendAppsScreenState::NO_SHOW);
  HandleSkip();
}

void RecommendAppsScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void RecommendAppsScreenHandler::LoadAppListInUI(const base::Value& app_list) {
  RecordUmaScreenState(RecommendAppsScreenState::SHOW);
  const ui::ResourceBundle& resource_bundle =
      ui::ResourceBundle::GetSharedInstance();
  std::string app_list_webview =
      features::IsNewOobeLayoutEnabled()
          ? resource_bundle.LoadDataResourceString(
                IDR_ARC_SUPPORT_RECOMMEND_APP_LIST_VIEW_NEW_HTML)
          : resource_bundle.LoadDataResourceString(
                IDR_ARC_SUPPORT_RECOMMEND_APP_LIST_VIEW_HTML);
  CallJS("login.RecommendAppsScreen.setWebview", app_list_webview);
  CallJS("login.RecommendAppsScreen.loadAppList", app_list);
}

void RecommendAppsScreenHandler::OnUserSkip() {
  RecordUmaScreenAction(RecommendAppsScreenAction::SKIPPED);
  HandleSkip();
}

// There are three scenarios that HandleSkip() is called:
// 1. The user clicks the Skip button.
// 2. The user doesn't select any apps and click the Install button.
// 3. The response from the fetcher cannot be parsed.
// Each case has its own entry point to be logged.
void RecommendAppsScreenHandler::HandleSkip() {
  if (screen_)
    screen_->OnSkip();
}

void RecommendAppsScreenHandler::HandleInstall(const base::ListValue* args) {
  if (recommended_app_count_ != 0) {
    int selected_app_count = static_cast<int>(args->GetSize());
    int selected_recommended_percentage =
        100 * selected_app_count / recommended_app_count_;
    RecordUmaUserSelectionAppCount(selected_app_count);
    RecordUmaSelectedRecommendedPercentage(selected_recommended_percentage);
  }

  // If the user does not select any apps, we should skip the app downloading
  // screen.
  if (args->GetList().empty()) {
    RecordUmaScreenAction(RecommendAppsScreenAction::SELECTED_NONE);
    HandleSkip();
    return;
  }

  RecordUmaScreenAction(RecommendAppsScreenAction::APP_SELECTED);
  pref_service_->Set(arc::prefs::kArcFastAppReinstallPackages, *args);

  arc::ArcFastAppReinstallStarter* fast_app_reinstall_starter =
      arc::ArcSessionManager::Get()->fast_app_resintall_starter();
  if (fast_app_reinstall_starter) {
    fast_app_reinstall_starter->OnAppsSelectionFinished();
  } else {
    LOG(ERROR)
        << "Cannot complete Fast App Reinstall flow. Starter is not available.";
  }

  if (screen_)
    screen_->OnInstall();
}

}  // namespace chromeos
