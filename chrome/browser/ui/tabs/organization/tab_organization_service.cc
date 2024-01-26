// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "components/sync/service/sync_user_settings.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

TabOrganizationService::TabOrganizationService(
    content::BrowserContext* browser_context)
    : SettingsEnabledObserver(optimization_guide::proto::ModelExecutionFeature::
                                  MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION),
      profile_(Profile::FromBrowserContext(browser_context)),
      optimization_guide_keyed_service_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)) {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_->AddModelExecutionSettingsEnabledObserver(
        this);
  }
  tab_sensitivity_cache_ = std::make_unique<TabSensitivityCache>(
      Profile::FromBrowserContext(browser_context));
  trigger_backoff_ =
      std::make_unique<ProfilePrefBackoffLevelProvider>(browser_context);
  trigger_observer_ = std::make_unique<TabOrganizationTriggerObserver>(
      base::BindRepeating(&TabOrganizationService::OnTriggerOccured,
                          base::Unretained(this)),
      browser_context, MakeTrigger(trigger_backoff_.get()));
}
TabOrganizationService::~TabOrganizationService() = default;

void TabOrganizationService::OnTriggerOccured(const Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    // If the organizations havent been fully accepted or rejected, then it does
    // not need to be reset.
    if (!GetSessionForBrowser(browser)->IsComplete()) {
      return;
    } else {
      RemoveBrowserFromSessionMap(browser);
    }
  }

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnToggleActionUIState(browser, true);
  }
}

const TabOrganizationSession* TabOrganizationService::GetSessionForBrowser(
    const Browser* browser) const {
  if (base::Contains(browser_session_map_, browser)) {
    return browser_session_map_.at(browser).get();
  }
  return nullptr;
}

TabOrganizationSession* TabOrganizationService::GetSessionForBrowser(
    const Browser* browser) {
  if (base::Contains(browser_session_map_, browser)) {
    return browser_session_map_.at(browser).get();
  }
  return nullptr;
}

TabOrganizationSession* TabOrganizationService::CreateSessionForBrowser(
    const Browser* browser,
    const content::WebContents* base_session_webcontents) {
  CHECK(!base::Contains(browser_session_map_, browser));

  std::pair<BrowserSessionMap::iterator, bool> pair =
      browser_session_map_.emplace(
          browser, TabOrganizationSession::CreateSessionForBrowser(
                       browser, base_session_webcontents));
  browser->tab_strip_model()->AddObserver(this);

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnSessionCreated(browser, pair.first->second.get());
  }

  return pair.first->second.get();
}

TabOrganizationSession* TabOrganizationService::ResetSessionForBrowser(
    const Browser* browser,
    const content::WebContents* base_session_webcontents) {
  browser->tab_strip_model()->RemoveObserver(this);
  if (base::Contains(browser_session_map_, browser)) {
    RemoveBrowserFromSessionMap(browser);
  }

  return CreateSessionForBrowser(browser, base_session_webcontents);
}

void TabOrganizationService::RestartSessionAndShowUI(
    const Browser* browser,
    const content::WebContents* base_session_webcontents) {
  ResetSessionForBrowser(browser, base_session_webcontents);
  StartRequestIfNotFRE(browser);
  OnUserInvokedFeature(browser);
}

void TabOrganizationService::OnUserInvokedFeature(const Browser* browser) {
  for (TabOrganizationObserver& observer : observers_) {
    observer.OnUserInvokedFeature(browser);
  }
}

bool TabOrganizationService::CanStartRequest() const {
  const syncer::SyncService* const sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (!sync_service) {
    return false;
  }

  // Sync must be enabled.
  if (!sync_service->IsSyncFeatureEnabled()) {
    return false;
  }

  // Sync must not be paused.
  if (!sync_service->IsSyncFeatureActive()) {
    return false;
  }

  // History Sync must be enabled.
  if (!sync_service->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory)) {
    return false;
  }

  return true;
}

void TabOrganizationService::StartRequestIfNotFRE(const Browser* browser) {
  const PrefService* pref_service = browser->profile()->GetPrefs();
  bool show_fre =
      pref_service->GetBoolean(tab_search_prefs::kTabOrganizationShowFRE);
  if (!show_fre) {
    StartRequest(browser);
  }
}

void TabOrganizationService::StartRequest(const Browser* browser) {
  if (!CanStartRequest()) {
    return;
  }

  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session || session->IsComplete()) {
    session = ResetSessionForBrowser(browser);
  }
  if (session->request()->state() ==
      TabOrganizationRequest::State::NOT_STARTED) {
    session->StartRequest();
  }
}

void TabOrganizationService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
    case TabStripModelChange::kReplaced: {
      return;
    }
    // When a tab is added or removed on the tabstrip destroy the session
    // for that browser.
    case TabStripModelChange::kInserted:
    case TabStripModelChange::kRemoved: {
      const auto find_result = std::find_if(
          browser_session_map_.begin(), browser_session_map_.end(),

          [&tab_strip_model](
              std::pair<const Browser* const,
                        std::unique_ptr<TabOrganizationSession>>& element) {
            return element.first->tab_strip_model() == tab_strip_model;
          });
      if (find_result != browser_session_map_.end()) {
        RemoveBrowserFromSessionMap(find_result->first);
      }
      return;
    }
  }
}

void TabOrganizationService::AcceptTabOrganization(
    Browser* browser,
    TabOrganization::ID session_id,
    TabOrganization::ID organization_id) {
  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session || session->session_id() != session_id) {
    return;
  }

  TabOrganization* organization = nullptr;
  for (const std::unique_ptr<TabOrganization>& maybe_organization :
       session->tab_organizations()) {
    if (maybe_organization->organization_id() == organization_id) {
      organization = maybe_organization.get();
      break;
    }
  }

  if (!organization) {
    return;
  }

  organization->Accept();

  // if the session is completed, then destroy it.
  if (session->IsComplete()) {
    RemoveBrowserFromSessionMap(browser);
  }

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnOrganizationAccepted(browser);
  }
}

void TabOrganizationService::OnActionUIAccepted(const Browser* browser) {
  StartRequestIfNotFRE(browser);
  OnUserInvokedFeature(browser);
  trigger_backoff_->Decrement();
}

void TabOrganizationService::OnActionUIDismissed(const Browser* browser) {
  trigger_backoff_->Increment();
}

void TabOrganizationService::Shutdown() {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_
        ->RemoveModelExecutionSettingsEnabledObserver(this);
    optimization_guide_keyed_service_ = nullptr;
  }
}

void TabOrganizationService::PrepareToEnableOnRestart() {
  // Ash-chrome uses a different FlagsStorage if the user is the owner. On
  // ChromeOS verifying if the owner is signed in is async operation.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile = profile_->GetOriginalProfile();
  // Chrome OS builds sometimes run on non-Chrome OS environments.
  if ((base::SysInfo::IsRunningOnChromeOS() ||
       skip_chrome_os_device_check_for_testing_) &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    ash::OwnerSettingsServiceAsh* service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile);
    service->IsOwnerAsync(base::BindOnce(
        &TabOrganizationService::EnableTabOrganizationFeaturesForChromeAsh,
        weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage =
      std::make_unique<flags_ui::PrefServiceFlagsStorage>(
          g_browser_process->local_state());
  EnableTabOrganizationFeatures(flags_storage.get());
}

void TabOrganizationService::RemoveBrowserFromSessionMap(
    const Browser* browser) {
  CHECK(base::Contains(browser_session_map_, browser));
  browser->tab_strip_model()->RemoveObserver(this);
  browser_session_map_.erase(browser);
}

void TabOrganizationService::EnableTabOrganizationFeatures(
    flags_ui::FlagsStorage* flags_storage) {
  about_flags::SetFeatureEntryEnabled(
      flags_storage,
      std::string(flag_descriptions::kChromeRefresh2023Id) +
          flags_ui::kMultiSeparatorChar +
          /*enable_feature_index=*/"1",
      true);
  about_flags::SetFeatureEntryEnabled(
      flags_storage,
      std::string(flag_descriptions::kChromeWebuiRefresh2023Id) +
          flags_ui::kMultiSeparatorChar +
          /*enable_feature_index=*/"1",
      true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::about_flags::FeatureFlagsUpdate(
      *flags_storage, profile_->GetOriginalProfile()->GetPrefs())
      .UpdateSessionManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TabOrganizationService::EnableTabOrganizationFeaturesForChromeAsh(
    bool is_owner) {
  Profile* original_profile = profile_->GetOriginalProfile();
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage;
  if (is_owner) {
    flags_storage = std::make_unique<ash::about_flags::OwnerFlagsStorage>(
        original_profile->GetPrefs(),
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile));
  } else {
    flags_storage = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        original_profile->GetPrefs());
  }
  EnableTabOrganizationFeatures(flags_storage.get());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
