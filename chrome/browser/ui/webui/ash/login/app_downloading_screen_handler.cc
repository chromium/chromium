// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

int GetNumberOfUserSelectedApps() {
  const Profile* profile = ProfileManager::GetActiveUserProfile();
  const PrefService* pref_service = profile->GetPrefs();
  return static_cast<int>(
      pref_service->GetList(arc::prefs::kArcFastAppReinstallPackages).size());
}

}  // namespace

namespace ash {

AppDownloadingScreenHandler::AppDownloadingScreenHandler()
    : BaseScreenHandler(kScreenId) {}

AppDownloadingScreenHandler::~AppDownloadingScreenHandler() = default;

void AppDownloadingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("appDownloadingScreenDescription",
                IDS_LOGIN_APP_DOWNLOADING_SCREEN_DESCRIPTION,
                ui::GetChromeOSDeviceName());
  builder->Add("appDownloadingContinueSetup",
               IDS_LOGIN_APP_DOWNLOADING_SCREEN_NEXT);
  builder->Add("appDownloadingScreenTitle",
               IDS_LOGIN_APP_DOWNLOADING_SCREEN_TITLE);
}

void AppDownloadingScreenHandler::Show() {
  base::Value::Dict data;
  data.Set("numOfApps", GetNumberOfUserSelectedApps());
  ShowInWebUI(std::move(data));
}

base::WeakPtr<AppDownloadingScreenView>
AppDownloadingScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
