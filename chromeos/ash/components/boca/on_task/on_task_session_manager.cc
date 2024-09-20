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
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace ash::boca {

namespace {

// Delay in seconds before we attempt to add a tab.
constexpr int kRetryAddTabTime = 3;

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
    std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager)
    : system_web_app_manager_(std::move(system_web_app_manager)),
      system_web_app_launch_helper_(
          std::make_unique<OnTaskSessionManager::SystemWebAppLaunchHelper>(
              system_web_app_manager_.get())) {}

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
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
  }
}

void OnTaskSessionManager::OnBundleUpdated(const ::boca::Bundle& bundle) {
  for (const ::boca::ContentConfig& content_config : bundle.content_configs()) {
    CHECK(content_config.has_url());
    const GURL url(content_config.url());
    OnTaskBlocklist::RestrictionLevel restriction_level;
    if (content_config.has_locked_navigation_options()) {
      ::boca::LockedNavigationOptions_NavigationType navigation_type =
          content_config.locked_navigation_options().navigation_type();
      restriction_level = NavigationTypeToRestrictionLevel(navigation_type);
    } else {
      restriction_level = OnTaskBlocklist::RestrictionLevel::kNoRestrictions;
    }
    // TODO (b/358197253): Stop the window tracker briefly while adding the new
    // tabs before resuming it.
    system_web_app_launch_helper_->AddTab(url, restriction_level);
  }
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(window_id);
    bool is_lock_mode = bundle.locked();
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(
        /*pinned=*/is_lock_mode, window_id);
  }
}

OnTaskSessionManager::SystemWebAppLaunchHelper::SystemWebAppLaunchHelper(
    OnTaskSystemWebAppManager* system_web_app_manager)
    : system_web_app_manager_(system_web_app_manager) {}

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
    OnTaskBlocklist::RestrictionLevel restriction_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launch_in_progress_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SystemWebAppLaunchHelper::AddTab,
                       weak_ptr_factory_.GetWeakPtr(), url, restriction_level),
        base::Seconds(kRetryAddTabTime));
    return;
  }
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->CreateBackgroundTabWithUrl(window_id, url,
                                                        restriction_level);
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

  // Facilitate seamless transition between bundle modes by pre-configuring
  // the Boca SWA.
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->SetWindowTrackerForSystemWebAppWindow(window_id);
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(
        /*pinned=*/true, window_id);
    system_web_app_manager_->SetPinStateForSystemWebAppWindow(
        /*pinned=*/false, window_id);
  }
}

}  // namespace ash::boca
