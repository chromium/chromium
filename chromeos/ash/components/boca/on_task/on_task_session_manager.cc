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
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "components/sessions/core/session_id.h"

namespace ash::boca {

OnTaskSessionManager::OnTaskSessionManager(
    std::unique_ptr<OnTaskSystemWebAppManager> system_web_app_manager)
    : system_web_app_manager_(std::move(system_web_app_manager)) {}

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

  system_web_app_manager_->LaunchSystemWebAppAsync(
      base::BindOnce(&OnTaskSessionManager::OnBocaSWALaunched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnTaskSessionManager::OnSessionEnded(const std::string& session_id) {
  if (const SessionID window_id =
          system_web_app_manager_->GetActiveSystemWebAppWindowID();
      window_id.is_valid()) {
    system_web_app_manager_->CloseSystemWebAppWindow(window_id);
  }
}

void OnTaskSessionManager::OnBocaSWALaunched(bool success) {
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
        /*pinned-*/ false, window_id);
  }
}

}  // namespace ash::boca
