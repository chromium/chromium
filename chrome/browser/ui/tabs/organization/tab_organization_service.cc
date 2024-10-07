// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/metrics.h"
#include "chrome/browser/ui/tabs/organization/request_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/organization/tab_sensitivity_cache.h"
#include "chrome/browser/ui/tabs/organization/trigger_policies.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

TabOrganizationService::TabOrganizationService(
    content::BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)) {
  tab_sensitivity_cache_ = std::make_unique<TabSensitivityCache>(
      Profile::FromBrowserContext(browser_context));
  trigger_backoff_ =
      std::make_unique<ProfilePrefBackoffLevelProvider>(browser_context);
  trigger_observer_ = std::make_unique<TabOrganizationTriggerObserver>(
      base::BindRepeating(&TabOrganizationService::OnTriggerOccured,
                          base::Unretained(this)),
      browser_context,
      MakeTrigger(trigger_backoff_.get(),
                  Profile::FromBrowserContext(browser_context)));
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
    const TabOrganizationEntryPoint entrypoint,
    const tabs::TabModel* base_session_tab) {
  CHECK(!base::Contains(browser_session_map_, browser));
  CHECK(browser->tab_strip_model()->SupportsTabGroups());
  std::pair<BrowserSessionMap::iterator, bool> pair =
      browser_session_map_.emplace(
          browser, TabOrganizationSession::CreateSessionForBrowser(
                       browser, entrypoint, base_session_tab));
  browser->tab_strip_model()->AddObserver(this);

  for (TabOrganizationObserver& observer : observers_) {
    observer.OnSessionCreated(browser, pair.first->second.get());
  }

  return pair.first->second.get();
}

TabOrganizationSession* TabOrganizationService::ResetSessionForBrowser(
    const Browser* browser,
    const TabOrganizationEntryPoint entrypoint,
    const tabs::TabModel* base_session_tab) {
  browser->tab_strip_model()->RemoveObserver(this);
  if (base::Contains(browser_session_map_, browser)) {
    RemoveBrowserFromSessionMap(browser);
  }

  return CreateSessionForBrowser(browser, entrypoint, base_session_tab);
}

void TabOrganizationService::RestartSessionAndShowUI(
    const Browser* browser,
    const TabOrganizationEntryPoint entrypoint,
    const tabs::TabModel* base_session_tab) {
  ResetSessionForBrowser(browser, entrypoint, base_session_tab);
  StartRequestIfNotFRE(browser, entrypoint);
  OnUserInvokedFeature(browser);
}

void TabOrganizationService::OnUserInvokedFeature(const Browser* browser) {
  for (TabOrganizationObserver& observer : observers_) {
    observer.OnUserInvokedFeature(browser);
  }
}

bool TabOrganizationService::CanStartRequest() const {
  CHECK(TabOrganizationUtils::GetInstance()->IsEnabled(profile_));

// The signin flow is not used on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  const signin::IdentityManager* const identity_manager(
      IdentityManagerFactory::GetInstance()->GetForProfile(profile_));
  const auto primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const auto extended_account_info =
      identity_manager->FindExtendedAccountInfo(primary_account_info);
  return !extended_account_info.IsEmpty();
#else
  return true;
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

void TabOrganizationService::StartRequestIfNotFRE(
    const Browser* browser,
    const TabOrganizationEntryPoint entrypoint) {
  const PrefService* pref_service = browser->profile()->GetPrefs();
  bool show_fre =
      pref_service->GetBoolean(tab_search_prefs::kTabOrganizationShowFRE);
  if (!show_fre) {
    StartRequest(browser, entrypoint);
  }
}

void TabOrganizationService::StartRequest(
    const Browser* browser,
    const TabOrganizationEntryPoint entrypoint) {
  if (!CanStartRequest()) {
    return;
  }

  TabOrganizationSession* session = GetSessionForBrowser(browser);
  if (!session || session->IsComplete()) {
    session = ResetSessionForBrowser(browser, entrypoint);
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
      const Browser* browser = GetBrowserForTabStripModel(tab_strip_model);
      if (browser) {
        RemoveBrowserFromSessionMap(browser);
      }
      return;
    }
  }
}

void TabOrganizationService::OnTabGroupChanged(const TabGroupChange& change) {
  const Browser* browser = GetBrowserForTabStripModel(change.model);
  if (!browser) {
    return;
  }
  TabOrganizationSession* session = GetSessionForBrowser(browser);
  CHECK(session);
  // Ignore changes when the session has already been accepted, to avoid acting
  // on changes made by the session itself.
  if (session->request()->state() == TabOrganizationRequest::State::COMPLETED) {
    return;
  }

  switch (change.type) {
    case TabGroupChange::kMoved:
    case TabGroupChange::kEditorOpened: {
      return;
    }
    // When a tab group's name has changed, destroy the session for that
    // browser. Ignore color changes, as they do not affect tab organization
    // data.
    case TabGroupChange::kVisualsChanged: {
      const TabGroupChange::VisualsChange* visuals_change =
          change.GetVisualsChange();
      if (visuals_change->old_visuals->title() !=
          visuals_change->new_visuals->title()) {
        RemoveBrowserFromSessionMap(browser);
      }
      return;
    }
    // When a tab group is added or removed on the tabstrip, or its contents
    // changes, destroy the session for that browser.
    case TabGroupChange::kCreated:
    case TabGroupChange::kContentsChanged:
    case TabGroupChange::kClosed: {
      RemoveBrowserFromSessionMap(browser);
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
  StartRequestIfNotFRE(browser, TabOrganizationEntryPoint::kProactive);
  OnUserInvokedFeature(browser);
  trigger_backoff_->Decrement();
}

void TabOrganizationService::OnActionUIDismissed(const Browser* browser) {
  trigger_backoff_->Increment();
}

void TabOrganizationService::RemoveBrowserFromSessionMap(
    const Browser* browser) {
  CHECK(base::Contains(browser_session_map_, browser));
  browser->tab_strip_model()->RemoveObserver(this);
  browser_session_map_.erase(browser);
}

const Browser* TabOrganizationService::GetBrowserForTabStripModel(
    const TabStripModel* tab_strip_model) {
  const auto find_result = std::find_if(
      browser_session_map_.begin(), browser_session_map_.end(),

      [&tab_strip_model](
          std::pair<const Browser* const,
                    std::unique_ptr<TabOrganizationSession>>& element) {
        return element.first->tab_strip_model() == tab_strip_model;
      });
  if (find_result == browser_session_map_.end()) {
    return nullptr;
  }
  return find_result->first;
}
