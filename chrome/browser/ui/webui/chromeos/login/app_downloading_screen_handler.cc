// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/app_downloading_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int GetNumberOfUserSelectedApps() {
  const Profile* profile = ProfileManager::GetActiveUserProfile();
  const PrefService* pref_service = profile->GetPrefs();
  return static_cast<int>(
      pref_service->Get(arc::prefs::kArcFastAppReinstallPackages)
          ->GetList()
          .size());
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId AppDownloadingScreenView::kScreenId;

AppDownloadingScreenHandler::AppDownloadingScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.AppDownloadingScreen.userActed");
}

AppDownloadingScreenHandler::~AppDownloadingScreenHandler() {}

void AppDownloadingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("appDownloadingScreenTitleSingular",
               IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE_SINGULAR);
  builder->Add("appDownloadingScreenTitlePlural",
               IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE_PLURAL);
  builder->Add("appDownloadingScreenDescription",
               IDS_LOGIN_APP_DOWNLOADING_SCREEN_DESCRIPTION);
  builder->Add("appDownloadingContinueSetup",
               IDS_LOGIN_APP_DOWNLOADING_CONTINUE_SETUP);
}

void AppDownloadingScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
}

void AppDownloadingScreenHandler::Bind(AppDownloadingScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void AppDownloadingScreenHandler::Show() {
  ShowScreen(kScreenId);
  CallJS("login.AppDownloadingScreen.updateNumberOfSelectedApps",
         base::Value(GetNumberOfUserSelectedApps()));
}

void AppDownloadingScreenHandler::Hide() {}

void AppDownloadingScreenHandler::Initialize() {}

}  // namespace chromeos
