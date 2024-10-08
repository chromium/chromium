// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/graduation/graduation_manager_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/edusumer/graduation_utils.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"

namespace ash::graduation {

GraduationManagerImpl::GraduationManagerImpl() {
  // SessionManager may be unset in unit tests.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager_observation_.Observe(session_manager);
  }
}

GraduationManagerImpl::~GraduationManagerImpl() {
  pref_change_registrar_.Reset();
}

const std::string GraduationManagerImpl::GetLanguageCode() const {
  return google_util::GetGoogleLocale(
      g_browser_process->GetApplicationLocale());
}

void GraduationManagerImpl::OnUserSessionStarted(bool is_primary) {
  profile_ = ProfileManager::GetActiveUserProfile();
  CHECK(profile_);
  if (!profile_->GetProfilePolicyConnector()->IsManaged()) {
    return;
  }

  nudge_controller_ =
      std::make_unique<GraduationNudgeController>(profile_->GetPrefs());

  SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile_);
  CHECK(swa_manager);
  swa_manager->on_apps_synchronized().Post(
      FROM_HERE, base::BindOnce(&GraduationManagerImpl::OnAppsSynchronized,
                                weak_ptr_factory_.GetWeakPtr()));
}

void GraduationManagerImpl::OnAppsSynchronized() {
  CHECK(profile_);
  auto* web_app_provider = SystemWebAppManager::GetWebAppProvider(profile_);
  CHECK(web_app_provider);
  web_app_provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&GraduationManagerImpl::OnWebAppProviderReady,
                                weak_ptr_factory_.GetWeakPtr()));
}

void GraduationManagerImpl::OnWebAppProviderReady() {
  // Set initial app pinned state.
  UpdateAppPinnedState();

  PrefService* pref_service = profile_->GetPrefs();
  CHECK(pref_service);
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kGraduationEnablementStatus,
      base::BindRepeating(&GraduationManagerImpl::UpdateAppPinnedState,
                          base::Unretained(this)));
}

void GraduationManagerImpl::UpdateAppPinnedState() {
  CHECK(profile_);
  SystemWebAppManager* swa_manager = SystemWebAppManager::Get(profile_);

  CHECK(swa_manager);
  if (!swa_manager->IsSystemWebApp(web_app::kGraduationAppId)) {
    return;
  }

  bool is_policy_enabled = IsEligibleForGraduation(profile_->GetPrefs());
  if (is_policy_enabled) {
    PinAppWithIDToShelf(web_app::kGraduationAppId);
    nudge_controller_->MaybeShowNudge(ash::ShelfID(web_app::kGraduationAppId));
    return;
  }

  UnpinAppWithIDFromShelf(web_app::kGraduationAppId);
  nudge_controller_->ResetNudgePref();
  auto* browser =
      FindSystemWebAppBrowser(profile_, SystemWebAppType::GRADUATION);
  if (browser) {
    browser->window()->Close();
  }
}
}  // namespace ash::graduation
