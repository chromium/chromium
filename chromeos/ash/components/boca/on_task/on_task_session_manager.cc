// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/boca/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace ash::boca {

namespace {

// Delay in seconds before we attempt to add a tab.
constexpr base::TimeDelta kAddTabRetryDelay = base::Seconds(3);

// Delay in seconds before we attempt to remove a tab.
constexpr base::TimeDelta kRemoveTabRetryDelay = base::Seconds(3);

OnTaskBlocklist::RestrictionLevel NavigationTypeToRestrictionLevel(
    ::boca::LockedNavigationOptions::NavigationType navigation_type) {
  switch (navigation_type) {
    case ::boca::LockedNavigationOptions::OPEN_NAVIGATION:
      return OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
    case ::boca::LockedNavigationOptions::BLOCK_NAVIGATION:
      return OnTaskBlocklist::RestrictionLevel::kLimitedNavigation;
    case ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION:
      return OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation;
    case ::boca::LockedNavigationOptions::LIMITED_NAVIGATION:
      return OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation;
    default:
      return OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
  }
}

}  // namespace

OnTaskSessionManager::OnTaskSessionManager(
    std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager,
    std::unique_ptr<OnTaskExtensionsManager> extensions_manager)
    : system_web_app_manager_(std::move(system_web_app_manager)),
      extensions_manager_(std::move(extensions_manager)),
      system_web_app_launch_helper_(
          std::make_unique<OnTaskSessionManager::SystemWebAppLaunchHelper>(
              system_web_app_manager_.get(),
              &active_tab_tracker_)) {}

OnTaskSessionManager::~OnTaskSessionManager() = default;

void OnTaskSessionManager::OnSessionStarted(
    const std::string& session_id,
    const ::boca::UserIdentity& producer) {
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    // Close all pre-existing SWA instances before we reopen a new one to set
    // things up for OnTask. We should rarely get here because relevant
    // notifiers ensure the SWA is closed at the onset of a session.
    //
    // TODO (b/354007279): Look out for and break from loop should window close
    // fail more than once.
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
    OnSessionStarted(session_id, producer);
    return;
  }
  system_web_app_launch_helper_->LaunchBocaSWA();
}

void OnTaskSessionManager::OnSessionEnded(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
  }
  provider_url_tab_ids_map_.clear();

  // Re-enable extensions on session end to prepare for subsequent sessions.
  extensions_manager_->ReEnableExtensions();
}

void OnTaskSessionManager::OnBundleUpdated(const ::boca::Bundle& bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<GURL> current_urls_set;
  for (const ::boca::ContentConfig& content_config : bundle.content_configs()) {
    CHECK(content_config.has_url());
    const GURL url(content_config.url());
    current_urls_set.insert(url);

    // No need to add the tab if the tab is already tracked as opened in the
    // SWA.
    if (provider_url_tab_ids_map_.contains(url)) {
      continue;
    }
    OnTaskBlocklist::RestrictionLevel restriction_level;
    if (content_config.has_locked_navigation_options()) {
      ::boca::LockedNavigationOptions_NavigationType navigation_type =
          content_config.locked_navigation_options().navigation_type();
      restriction_level = NavigationTypeToRestrictionLevel(navigation_type);
    } else {
      restriction_level = OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
    }
    system_web_app_launch_helper_->AddTab(
        url, restriction_level,
        base::BindOnce(&OnTaskSessionManager::OnTabAdded,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }

  for (auto const& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    if (!current_urls_set.contains(provider_sent_url)) {
      system_web_app_launch_helper_->RemoveTab(
          tab_ids,
          base::BindOnce(&OnTaskSessionManager::OnTabRemoved,
                         weak_ptr_factory_.GetWeakPtr(), provider_sent_url));
    }
  }

  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    // TODO (b/370871395): Move `SetWindowTrackerForSystemWebAppWindow` to
    // `OnTaskSystemWebAppManager`.
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(
        window_id, &active_tab_tracker_);

    // Disable extensions in the context of OnTask before the window is locked.
    // Re-enable them otherwise.
    bool should_lock_window = bundle.locked();
    if (should_lock_window) {
      extensions_manager_->DisableExtensions();
    } else {
      extensions_manager_->ReEnableExtensions();
    }

    // Set appropriate pin state on the active window.
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(
        /*pinned=*/should_lock_window, window_id);
  }
}

OnTaskSessionManager::SystemWebAppLaunchHelper::SystemWebAppLaunchHelper(
    OnTaskSystemWebAppManager* system_web_app_manager,
    ActiveTabTracker* active_tab_tracker)
    : system_web_app_manager_(system_web_app_manager),
      active_tab_tracker_(active_tab_tracker) {}

OnTaskSessionManager::SystemWebAppLaunchHelper::~SystemWebAppLaunchHelper() =
    default;

void OnTaskSessionManager::SystemWebAppLaunchHelper::LaunchBocaSWA() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  launch_in_progress_ = true;
  system_web_app_manager_->LaunchSystemWebAppAsync(
      base::BindOnce(&SystemWebAppLaunchHelper::OnBocaSWALaunched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnTaskSessionManager::SystemWebAppLaunchHelper::AddTab(
    GURL url,
    OnTaskBlocklist::RestrictionLevel restriction_level,
    base::OnceCallback<void(SessionID)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launch_in_progress_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SystemWebAppLaunchHelper::AddTab,
                       weak_ptr_factory_.GetWeakPtr(), url, restriction_level,
                       std::move(callback)),
        kAddTabRetryDelay);
    return;
  }
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    const SessionID tab_id =
        system_web_app_manager_->CreateBackgroundTabWithUrl(window_id, url,
                                                            restriction_level);
    std::move(callback).Run(tab_id);
  }
}

void OnTaskSessionManager::SystemWebAppLaunchHelper::RemoveTab(
    const base::flat_set<SessionID>& tab_ids_to_remove,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launch_in_progress_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SystemWebAppLaunchHelper::RemoveTab,
                       weak_ptr_factory_.GetWeakPtr(), tab_ids_to_remove,
                       std::move(callback)),
        kRemoveTabRetryDelay);
    return;
  }
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->RemoveTabsWithTabIds(window_id, tab_ids_to_remove);
    std::move(callback).Run();
  }
}

void OnTaskSessionManager::SystemWebAppLaunchHelper::OnBocaSWALaunched(
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  launch_in_progress_ = false;
  if (!success) {
    // TODO(b/354007279): Enforce appropriate retries.
    return;
  }

  // Set up window tracker for the newly launched Boca SWA.
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    // TODO (b/370871395): Move `SetWindowTrackerForSystemWebAppWindow` to
    // `OnTaskSystemWebAppManager`.
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(
        window_id, active_tab_tracker_);
  }
}

void OnTaskSessionManager::OnTabAdded(GURL url, SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_id.is_valid()) {
    base::flat_set<SessionID>& tab_ids = provider_url_tab_ids_map_[url];
    tab_ids.insert(tab_id);
  }
}

void OnTaskSessionManager::OnTabRemoved(GURL url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (provider_url_tab_ids_map_.contains(url)) {
    // TODO(b/368105857): Remove child tabs.
    provider_url_tab_ids_map_.erase(url);
  }
}

}  // namespace ash::boca
