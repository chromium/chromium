// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>
#include <optional>

#include "ash/constants/notifier_catalogs.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/on_task/activity/active_tab_tracker.h"
#include "chromeos/ash/components/boca/on_task/notification_constants.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_notifications_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using message_center::NotifierId;
using message_center::NotifierType;

namespace ash::boca {
namespace {

// Delay in seconds before we attempt to add a tab.
constexpr base::TimeDelta kAddTabRetryDelay = base::Seconds(1);

// Delay in seconds before we attempt to remove a tab.
constexpr base::TimeDelta kRemoveTabRetryDelay = base::Seconds(1);

// Delay in seconds before we attempt to pin or unpin the active SWA window.
constexpr base::TimeDelta kSetPinnedStateDelay = base::Seconds(3);

}  // namespace

OnTaskSessionManager::OnTaskSessionManager(
    std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager,
    std::unique_ptr<OnTaskExtensionsManager> extensions_manager)
    : system_web_app_manager_(std::move(system_web_app_manager)),
      extensions_manager_(std::move(extensions_manager)),
      system_web_app_launch_helper_(
          std::make_unique<OnTaskSessionManager::SystemWebAppLaunchHelper>(
              system_web_app_manager_.get(),
              std::vector<boca::BocaWindowObserver*>{&active_tab_tracker_,
                                                     this})),
      notifications_manager_(OnTaskNotificationsManager::Create()) {}

OnTaskSessionManager::~OnTaskSessionManager() = default;

void OnTaskSessionManager::OnSessionStarted(
    const std::string& session_id,
    const ::boca::UserIdentity& producer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_session_id_ = session_id;
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    // Prepare the pre-existing Boca SWA instance for OnTask.
    system_web_app_manager_->PrepareSystemWebAppWindowForOnTask(window_id);
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(
        window_id, {&active_tab_tracker_, this});
  } else {
    system_web_app_launch_helper_->LaunchBocaSWA();
  }
}

void OnTaskSessionManager::OnSessionEnded(const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
  }
  active_session_id_ = std::nullopt;
  provider_url_tab_ids_map_.clear();
  provider_url_restriction_level_map_.clear();
  should_lock_window_ = false;

  // Re-enable extensions on session end to prepare for subsequent sessions.
  extensions_manager_->ReEnableExtensions();

  // Surface notification to notify user about session end.
  OnTaskNotificationsManager::NotificationCreateParams
      notification_create_params(
          kOnTaskSessionEndNotificationId,
          /*title=*/l10n_util::GetStringUTF16(IDS_ON_TASK_NOTIFICATION_TITLE),
          /*message=*/
          l10n_util::GetStringUTF16(
              IDS_ON_TASK_SESSION_END_NOTIFICATION_MESSAGE),
          /*notifier_id=*/
          NotifierId(NotifierType::SYSTEM_COMPONENT, kOnTaskNotifierId,
                     ash::NotificationCatalogName::kOnTaskSessionEnd));
  notifications_manager_->CreateNotification(
      std::move(notification_create_params));
}

void OnTaskSessionManager::OnBundleUpdated(const ::boca::Bundle& bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the Boca SWA is closed, we launch it again so we can apply bundle
  // updates. We clear `provider_url_tab_ids_map_` so we reopen all tabs from
  // the latest bundle.
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      !window_id.is_valid()) {
    provider_url_tab_ids_map_.clear();
    provider_url_restriction_level_map_.clear();
    system_web_app_launch_helper_->LaunchBocaSWA();
  }

  // Process bundle content.
  bool has_new_content = false;
  base::flat_set<GURL> current_urls_set;
  active_tab_url_ = GURL();
  for (const ::boca::ContentConfig& content_config : bundle.content_configs()) {
    CHECK(content_config.has_url());
    const GURL url(content_config.url());
    current_urls_set.insert(url);

    ::boca::LockedNavigationOptions::NavigationType restriction_level;
    if (content_config.has_locked_navigation_options()) {
      ::boca::LockedNavigationOptions_NavigationType navigation_type =
          content_config.locked_navigation_options().navigation_type();
      restriction_level = navigation_type;
    } else {
      restriction_level = ::boca::LockedNavigationOptions::OPEN_NAVIGATION;
    }

    // No need to add the tab if the tab is already tracked as opened in the
    // SWA and the restriction levels are the same.
    if (provider_url_tab_ids_map_.contains(url)) {
      if (provider_url_restriction_level_map_[url] == restriction_level) {
        continue;
      }

      if (active_tab_url_.is_empty()) {
        const SessionID tab_id = system_web_app_manager_->GetActiveTabID();
        TrackActiveTabURLFromTab(tab_id);
      }
      // Close the tab and any child tabs associated with the given url.
      // TODO(crbug.com/373961026): Remove tabs for restriction updates that
      // went to a stricter setting.
      system_web_app_launch_helper_->RemoveTab(
          provider_url_tab_ids_map_[url],
          base::BindOnce(&OnTaskSessionManager::OnBundleTabRemoved,
                         weak_ptr_factory_.GetWeakPtr(), url));
    }

    has_new_content = true;
    system_web_app_launch_helper_->AddTab(
        url, restriction_level,
        base::BindOnce(&OnTaskSessionManager::OnBundleTabAdded,
                       weak_ptr_factory_.GetWeakPtr(), url, restriction_level));
  }

  bool has_removed_content = false;
  for (auto const& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    if (!current_urls_set.contains(provider_sent_url)) {
      has_removed_content = true;
      system_web_app_launch_helper_->RemoveTab(
          tab_ids,
          base::BindOnce(&OnTaskSessionManager::OnBundleTabRemoved,
                         weak_ptr_factory_.GetWeakPtr(), provider_sent_url));
    }
  }

  LockOrUnlockWindow(bundle.locked());

  // Show relevant notifications if content was added or deleted.
  if (has_new_content) {
    OnTaskNotificationsManager::NotificationCreateParams
        notification_create_params(
            kOnTaskBundleContentAddedNotificationId,
            /*title=*/l10n_util::GetStringUTF16(IDS_ON_TASK_NOTIFICATION_TITLE),
            /*message=*/
            l10n_util::GetStringUTF16(IDS_ON_TASK_BUNDLE_CONTENT_ADDED_MESSAGE),
            /*notifier_id=*/
            NotifierId(
                NotifierType::SYSTEM_COMPONENT, kOnTaskNotifierId,
                ash::NotificationCatalogName::kOnTaskAddContentToBundle));
    notifications_manager_->CreateNotification(
        std::move(notification_create_params));
  }
  if (has_removed_content) {
    OnTaskNotificationsManager::NotificationCreateParams
        notification_create_params(
            kOnTaskBundleContentRemovedNotificationId,
            /*title=*/l10n_util::GetStringUTF16(IDS_ON_TASK_NOTIFICATION_TITLE),
            /*message=*/
            l10n_util::GetStringUTF16(
                IDS_ON_TASK_BUNDLE_CONTENT_REMOVED_MESSAGE),
            /*notifier_id=*/
            NotifierId(
                NotifierType::SYSTEM_COMPONENT, kOnTaskNotifierId,
                ash::NotificationCatalogName::kOnTaskRemoveContentFromBundle));
    notifications_manager_->CreateNotification(
        std::move(notification_create_params));
  }
}

void OnTaskSessionManager::OnAppReloaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const SessionID window_id =
      system_web_app_manager_->GetActiveSystemWebAppWindowID();
  if (!window_id.is_valid()) {
    // No active window found, so we return. We should rarely get here.
    return;
  }

  // Only restore tabs and set up window tracker if there is an active session.
  // This ensures we do not inadvertently block URLs.
  if (!active_session_id_.has_value()) {
    return;
  }
  system_web_app_manager_->PrepareSystemWebAppWindowForOnTask(window_id);
  system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(
      window_id, {&active_tab_tracker_, this});

  // Reopen only content that was originally shared by the provider. We also
  // clear stale tab ids that were tracked with the previous instance.
  for (auto& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    tab_ids.clear();
    ::boca::LockedNavigationOptions::NavigationType restriction_level =
        ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION;  // Default
                                                             // restriction.
    if (provider_url_restriction_level_map_.contains(provider_sent_url)) {
      restriction_level =
          provider_url_restriction_level_map_[provider_sent_url];
    }
    system_web_app_launch_helper_->AddTab(
        provider_sent_url, restriction_level,
        base::BindOnce(&OnTaskSessionManager::OnBundleTabAdded,
                       weak_ptr_factory_.GetWeakPtr(), provider_sent_url,
                       restriction_level));
  }

  // Also lock window if necessary.
  LockOrUnlockWindow(should_lock_window_);
}

void OnTaskSessionManager::LockOrUnlockWindow(bool lock_window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool locked_mode_state_changed = (should_lock_window_ != lock_window);
  should_lock_window_ = lock_window;
  notifications_manager_->ConfigureForLockedMode(should_lock_window_);
  if (should_lock_window_) {
    extensions_manager_->DisableExtensions();
    if (locked_mode_state_changed) {
      // Show notification before locking the window.
      OnTaskNotificationsManager::NotificationCreateParams
          notification_create_params(
              kOnTaskEnterLockedModeNotificationId,
              /*title=*/
              l10n_util::GetStringUTF16(IDS_ON_TASK_NOTIFICATION_TITLE),
              /*message=*/
              l10n_util::GetStringUTF16(
                  IDS_ON_TASK_ENTER_LOCKED_MODE_NOTIFICATION_MESSAGE),
              /*notifier_id=*/
              NotifierId(NotifierType::SYSTEM_COMPONENT, kOnTaskNotifierId,
                         ash::NotificationCatalogName::kOnTaskEnterLockedMode));
      notifications_manager_->CreateNotification(
          std::move(notification_create_params));
    }
    // Attempt to lock the window. This should be a no-op should the window be
    // already locked.
    system_web_app_launch_helper_->SetPinStateForActiveSWAWindow(
        /*pinned=*/true,
        base::BindRepeating(&OnTaskSessionManager::OnSetPinStateOnBocaSWAWindow,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Re-enable extensions before attempting to unlock the window.
    extensions_manager_->ReEnableExtensions();
    system_web_app_launch_helper_->SetPinStateForActiveSWAWindow(
        /*pinned=*/false,
        base::BindRepeating(&OnTaskSessionManager::OnSetPinStateOnBocaSWAWindow,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void OnTaskSessionManager::OnTabAdded(const SessionID active_tab_id,
                                      const SessionID tab_id,
                                      const GURL url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tab_id.is_valid());
  if (!active_session_id_.has_value()) {
    // No active session. Close the tab after the tab creation has been
    // processed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<OnTaskSessionManager> instance,
               const SessionID window_id, const SessionID tab_id) {
              if (!instance) {
                return;
              }
              // Do not use `system_web_app_launch_helper_` here to ensure we
              // are working with the right window. This will result in a no-op
              // should the window be closed.
              instance->system_web_app_manager_->RemoveTabsWithTabIds(window_id,
                                                                      {tab_id});
            },
            weak_ptr_factory_.GetWeakPtr(),
            system_web_app_manager_->GetActiveSystemWebAppWindowID(), tab_id));
    return;
  }
  if (active_tab_id == tab_id) {
    return;
  }
  if (!active_tab_id.is_valid()) {
    provider_url_tab_ids_map_[url].insert(tab_id);
    return;
  }
  for (auto& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    // Guarantee that tabs sent by provider are not regarded as child tabs.
    if (tab_ids.contains(tab_id)) {
      return;
    }
    if (tab_ids.contains(active_tab_id)) {
      tab_ids.insert(tab_id);
      return;
    }
  }
}

void OnTaskSessionManager::OnTabRemoved(const SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tab_id.is_valid());
  for (auto& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    if (tab_ids.contains(tab_id)) {
      tab_ids.erase(tab_id);
      return;
    }
  }
}

OnTaskSessionManager::SystemWebAppLaunchHelper::SystemWebAppLaunchHelper(
    OnTaskSystemWebAppManager* system_web_app_manager,
    std::vector<boca::BocaWindowObserver*> observers)
    : system_web_app_manager_(system_web_app_manager), observers_(observers) {}

OnTaskSessionManager::SystemWebAppLaunchHelper::~SystemWebAppLaunchHelper() =
    default;

void OnTaskSessionManager::SystemWebAppLaunchHelper::LaunchBocaSWA() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launch_in_progress_) {
    // Another Boca SWA launch is in progress. Return.
    return;
  }
  launch_in_progress_ = true;
  system_web_app_manager_->LaunchSystemWebAppAsync(
      base::BindOnce(&SystemWebAppLaunchHelper::OnBocaSWALaunched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnTaskSessionManager::SystemWebAppLaunchHelper::AddTab(
    GURL url,
    ::boca::LockedNavigationOptions::NavigationType restriction_level,
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
    const std::set<SessionID>& tab_ids_to_remove,
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

void OnTaskSessionManager::SystemWebAppLaunchHelper::
    SetPinStateForActiveSWAWindow(bool pinned,
                                  base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launch_in_progress_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SystemWebAppLaunchHelper::SetPinStateForActiveSWAWindow,
                       weak_ptr_factory_.GetWeakPtr(), pinned,
                       std::move(callback)),
        kSetPinnedStateDelay);
    return;
  }
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(pinned,
                                                              window_id);
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
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(window_id,
                                                                   observers_);
  }
}

void OnTaskSessionManager::OnBundleTabAdded(
    GURL url,
    ::boca::LockedNavigationOptions::NavigationType restriction_level,
    SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tab_id.is_valid()) {
    // Ensure parent tab association with the right URL in case it is
    // accidentally added by `OnTabAdded` while observing new tab additions.
    for (const auto& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
      if (tab_ids.contains(tab_id)) {
        provider_url_tab_ids_map_[provider_sent_url].erase(tab_id);
        break;
      }
    }
    provider_url_tab_ids_map_[url].insert(tab_id);
    provider_url_restriction_level_map_[url] = restriction_level;

    // TODO(b/375538635): Revisit this logic when we open foreground tabs.
    if (active_tab_url_.is_valid() && url == active_tab_url_) {
      system_web_app_manager_->SwitchToTab(tab_id);
      active_tab_url_ = GURL();
    }
  }
}

void OnTaskSessionManager::OnBundleTabRemoved(GURL url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (provider_url_tab_ids_map_.contains(url)) {
    // TODO(b/368105857): Remove child tabs.
    provider_url_tab_ids_map_.erase(url);
    provider_url_restriction_level_map_.erase(url);
  }
}

void OnTaskSessionManager::OnSetPinStateOnBocaSWAWindow() {
  // TODO (b/370871395): Move `SetWindowTrackerForSystemWebAppWindow` to
  // `OnTaskSystemWebAppManager` eliminating the need for this callback.
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(
        window_id, {&active_tab_tracker_, this});
  }
}

void OnTaskSessionManager::TrackActiveTabURLFromTab(SessionID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tab_id.is_valid()) {
    active_tab_url_ = GURL();
  }
  for (const auto& [provider_sent_url, tab_ids] : provider_url_tab_ids_map_) {
    if (tab_ids.contains(tab_id)) {
      active_tab_url_ = provider_sent_url;
      break;
    }
  }
}

}  // namespace ash::boca
