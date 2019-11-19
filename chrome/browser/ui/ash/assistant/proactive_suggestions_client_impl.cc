// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/proactive_suggestions_client_impl.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/ash/assistant/proactive_suggestions_loader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace {

// Helpers ---------------------------------------------------------------------

syncer::SyncService* GetSyncService(Profile* profile) {
  return profile->IsSyncAllowed()
             ? ProfileSyncServiceFactory::GetForProfile(profile)
             : nullptr;
}

bool IsHistorySyncEnabledWithoutPassphrase(Profile* profile) {
  auto* sync_service = GetSyncService(profile);
  if (!sync_service)
    return false;

  const auto* user_settings = sync_service->GetUserSettings();
  if (!user_settings)
    return false;

  return user_settings->GetSelectedTypes().Has(
             syncer::UserSelectableType::kHistory) &&
         !user_settings->IsUsingSecondaryPassphrase();
}

}  // namespace

// ProactiveSuggestionsClientImpl ----------------------------------------------

ProactiveSuggestionsClientImpl::ProactiveSuggestionsClientImpl(
    Profile* profile)
    : profile_(profile) {
  // We observe Assistant state to detect enabling/disabling of Assistant in
  // settings as well as enabling/disabling of screen context.
  ash::AssistantState::Get()->AddObserver(this);

  // We observe the singleton BrowserList to receive events pertaining to the
  // currently active browser.
  BrowserList::AddObserver(this);

  // We observe the SyncService to detect changes in user sync settings which
  // may affect whether or not we will observe the active browser.
  auto* sync_service = GetSyncService(profile_);
  if (sync_service)
    sync_service->AddObserver(this);
}

ProactiveSuggestionsClientImpl::~ProactiveSuggestionsClientImpl() {
  if (delegate_)
    delegate_->OnProactiveSuggestionsClientDestroying();

  WebContentsObserver::Observe(nullptr);

  if (active_browser_)
    active_browser_->tab_strip_model()->RemoveObserver(this);

  auto* sync_service = GetSyncService(profile_);
  if (sync_service)
    sync_service->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
  ash::AssistantState::Get()->RemoveObserver(this);
}

void ProactiveSuggestionsClientImpl::SetDelegate(Delegate* delegate) {
  if (delegate == delegate_)
    return;

  delegate_ = delegate;

  // When a |delegate_| is set, we need to notify it of the active set of
  // proactive suggestions, if such a set exists. Failure to do so will leave
  // the |delegate_| without an update until the active browser context changes.
  if (delegate_ && active_proactive_suggestions_)
    delegate_->OnProactiveSuggestionsChanged(active_proactive_suggestions_);
}

void ProactiveSuggestionsClientImpl::OnBrowserRemoved(Browser* browser) {
  if (browser == active_browser_)
    SetActiveBrowser(nullptr);
}

void ProactiveSuggestionsClientImpl::OnBrowserSetLastActive(Browser* browser) {
  SetActiveBrowser(browser);
}

void ProactiveSuggestionsClientImpl::OnBrowserNoLongerActive(Browser* browser) {
  if (browser == active_browser_)
    SetActiveBrowser(nullptr);
}

void ProactiveSuggestionsClientImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // When the currently active browser tab changes, that indicates there has
  // been a change in the active contents.
  if (selection.active_tab_changed())
    SetActiveContents(tab_strip_model->GetActiveWebContents());
}

void ProactiveSuggestionsClientImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  SetActiveUrl(active_contents_->GetURL());
}

void ProactiveSuggestionsClientImpl::DidChangeVerticalScrollDirection(
    viz::VerticalScrollDirection scroll_direction) {
  if (delegate_)
    delegate_->OnSourceVerticalScrollDirectionChanged(scroll_direction);
}

void ProactiveSuggestionsClientImpl::OnAssistantFeatureAllowedChanged(
    ash::mojom::AssistantAllowedState state) {
  // When the Assistant feature is allowed/disallowed we may need to resume/
  // pause observation of the browser. We accomplish this by updating active
  // state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::OnAssistantSettingsEnabled(bool enabled) {
  // When Assistant is enabled/disabled in settings we may need to resume/pause
  // observation of the browser. We accomplish this by updating active state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::OnAssistantContextEnabled(bool enabled) {
  // When Assistant screen context is enabled/disabled in settings we may need
  // to resume/pause observation of the browser. We accomplish this by updating
  // active state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::OnStateChanged(syncer::SyncService* sync) {
  // When the state of the SyncService has changed, we may need to resume/pause
  // observation of the browser. We accomplish this by updating active state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::SetActiveBrowser(Browser* browser) {
  if (browser == active_browser_)
    return;

  // Clean up bindings on the previously active browser.
  if (active_browser_)
    active_browser_->tab_strip_model()->RemoveObserver(this);

  active_browser_ = browser;

  // We need to update active state to conditionally observe the active browser.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::SetActiveContents(
    content::WebContents* contents) {
  if (contents == active_contents_)
    return;

  active_contents_ = contents;

  // We observe the active contents so as to detect navigation changes.
  WebContentsObserver::Observe(active_contents_);

  // Perform an initial sync of the active url.
  SetActiveUrl(active_contents_ ? active_contents_->GetURL() : GURL());
}

void ProactiveSuggestionsClientImpl::SetActiveUrl(const GURL& url) {
  if (url == active_url_)
    return;

  active_url_ = url;

  // The previous set of proactive suggestions is no longer valid.
  SetActiveProactiveSuggestions(nullptr);

  // We only load proactive suggestions for http/https schemes.
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    loader_.reset();
    return;
  }

  // Start loading new proactive suggestions associated with the active url.
  loader_ = std::make_unique<ProactiveSuggestionsLoader>(profile_, active_url_);
  loader_->Start(base::BindOnce(
      &ProactiveSuggestionsClientImpl::SetActiveProactiveSuggestions,
      base::Unretained(this)));
}

void ProactiveSuggestionsClientImpl::SetActiveProactiveSuggestions(
    scoped_refptr<ash::ProactiveSuggestions> proactive_suggestions) {
  if (ash::ProactiveSuggestions::AreEqual(
          proactive_suggestions.get(), active_proactive_suggestions_.get())) {
    return;
  }

  active_proactive_suggestions_ = std::move(proactive_suggestions);

  if (delegate_)
    delegate_->OnProactiveSuggestionsChanged(active_proactive_suggestions_);
}

void ProactiveSuggestionsClientImpl::UpdateActiveState() {
  if (!active_browser_) {
    SetActiveContents(nullptr);
    return;
  }

  auto* tab_strip_model = active_browser_->tab_strip_model();

  // We never observe browsers that are off the record and we never observe
  // browsers when the Assistant feature is not allowed. We also don't observe
  // the browser when the user has disabled either Assistant or screen context
  // or when the user has disabled history sync or is using a sync passphrase.
  if (active_browser_->profile()->IsOffTheRecord() ||
      ash::AssistantState::Get()->allowed_state() !=
          ash::mojom::AssistantAllowedState::ALLOWED ||
      !ash::AssistantState::Get()->settings_enabled().value_or(false) ||
      !ash::AssistantState::Get()->context_enabled().value_or(false) ||
      !IsHistorySyncEnabledWithoutPassphrase(profile_)) {
    tab_strip_model->RemoveObserver(this);
    SetActiveContents(nullptr);
    return;
  }

  // We observe the tab strip associated with the active browser so as to detect
  // changes to the currently active tab.
  tab_strip_model->AddObserver(this);

  // Perform an initial sync of the active contents.
  SetActiveContents(tab_strip_model->GetActiveWebContents());
}
