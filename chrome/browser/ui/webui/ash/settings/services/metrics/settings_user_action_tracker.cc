// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/services/metrics/settings_user_action_tracker.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_sections.h"
#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"

namespace ash::settings {

SettingsUserActionTracker::SettingsUserActionTracker(
    Hierarchy* hierarchy,
    OsSettingsSections* sections,
    PrefService* profile_pref_service)
    : hierarchy_(hierarchy),
      sections_(sections),
      profile_pref_service_(profile_pref_service) {}

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
  per_session_tracker_ = std::make_unique<PerSessionSettingsUserActionTracker>(
      profile_pref_service_);
}

void SettingsUserActionTracker::EndCurrentSession() {
  // reset the pointers
  per_session_tracker_.reset();
  receiver_.reset();
}

void SettingsUserActionTracker::OnBindingDisconnected() {
  // Settings window is closed by the user, ending the current session.
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

// TODO(https://crbug.com/1133553): remove this once migration is complete.
void SettingsUserActionTracker::RecordSettingChange() {
  per_session_tracker_->RecordSettingChange();
}

void SettingsUserActionTracker::RecordSettingChangeWithDetails(
    chromeos::settings::mojom::Setting setting,
    mojom::SettingChangeValuePtr value) {
  per_session_tracker_->RecordSettingChange(setting);

  // Get the primary section location of the changed setting and log the metric.
  chromeos::settings::mojom::Section section_id =
      hierarchy_->GetSettingMetadata(setting).primary.section;
  const OsSettingsSection* section = sections_->GetSection(section_id);
  // new_value is initialized as null. Null value is used in cases that don't
  // require extra metadata.
  base::Value new_value;
  if (value) {
    if (value->is_bool_value()) {
      new_value = base::Value(value->get_bool_value());
    } else if (value->is_int_value()) {
      new_value = base::Value(value->get_int_value());
    } else if (value->is_string_value()) {
      new_value = base::Value(value->get_string_value());
    }
  }
  section->LogMetric(setting, new_value);

  base::UmaHistogramSparse("ChromeOS.Settings.SettingChanged",
                           static_cast<int>(setting));
}

}  // namespace ash::settings
