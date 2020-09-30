// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/settings_user_action_tracker.h"

#include <memory>
#include <utility>

#include "base/bind.h"

namespace chromeos {
namespace settings {

SettingsUserActionTracker::SettingsUserActionTracker() = default;

SettingsUserActionTracker::~SettingsUserActionTracker() = default;

void SettingsUserActionTracker::BindInterface(
    mojo::PendingReceiver<mojom::UserActionRecorder> pending_receiver) {
  // Only one user session should be active at a time.
  EndCurrentSession();
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&SettingsUserActionTracker::OnBindingDisconnected,
                     base::Unretained(this)));

  // New session started, so create a new per session tracker.
  per_session_tracker_ =
      std::make_unique<PerSessionSettingsUserActionTracker>();
}

void SettingsUserActionTracker::EndCurrentSession() {
  // Session ended, so delete the per session tracker.
  per_session_tracker_.reset();
  receiver_.reset();
}

void SettingsUserActionTracker::OnBindingDisconnected() {
  EndCurrentSession();
}

void SettingsUserActionTracker::RecordPageFocus() {
  per_session_tracker_->RecordPageFocus();
}

void SettingsUserActionTracker::RecordPageBlur() {
  per_session_tracker_->RecordPageBlur();
}

void SettingsUserActionTracker::RecordClick() {
  per_session_tracker_->RecordClick();
}

void SettingsUserActionTracker::RecordNavigation() {
  per_session_tracker_->RecordNavigation();
}

void SettingsUserActionTracker::RecordSearch() {
  per_session_tracker_->RecordSearch();
}

void SettingsUserActionTracker::RecordSettingChange() {
  per_session_tracker_->RecordSettingChange();
}

}  // namespace settings
}  // namespace chromeos
