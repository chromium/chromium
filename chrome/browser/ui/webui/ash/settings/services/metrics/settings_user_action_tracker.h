// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_SETTINGS_USER_ACTION_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_SETTINGS_USER_ACTION_TRACKER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/user_action_recorder.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/services/metrics/per_session_settings_user_action_tracker.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::settings {

class Hierarchy;
class OsSettingsSections;

// Records user actions within Settings. Utilizes a per session tracker that
// measures the user's effort required to change a setting. Eventually uses
// a per section tracker to record metrics in each section.
class SettingsUserActionTracker : public mojom::UserActionRecorder {
 public:
  SettingsUserActionTracker(Hierarchy* hierarchy,
                            OsSettingsSections* sections,
                            PrefService* profile_pref_service);
  SettingsUserActionTracker(const SettingsUserActionTracker& other) = delete;
  SettingsUserActionTracker& operator=(const SettingsUserActionTracker& other) =
      delete;
  ~SettingsUserActionTracker() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::UserActionRecorder> pending_receiver);

 private:
  // For unit tests.
  // SettingsUserActionTracker();
  friend class SettingsUserActionTrackerTest;
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedBool);
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedInt);
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedString);
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedBoolPref);
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedIntPref);
  FRIEND_TEST_ALL_PREFIXES(SettingsUserActionTrackerTest,
                           TestRecordSettingChangedNullValue);

  // mojom::UserActionRecorder:
  void RecordPageFocus() override;
  void RecordPageBlur() override;
  void RecordClick() override;
  void RecordNavigation() override;
  void RecordSearch() override;
  void RecordSettingChange() override;
  void RecordSettingChangeWithDetails(
      chromeos::settings::mojom::Setting setting,
      mojom::SettingChangeValuePtr value) override;

  void EndCurrentSession();
  void OnBindingDisconnected();

  raw_ptr<Hierarchy> hierarchy_;
  raw_ptr<OsSettingsSections> sections_;
  raw_ptr<PrefService> profile_pref_service_;

  std::unique_ptr<PerSessionSettingsUserActionTracker> per_session_tracker_;
  mojo::Receiver<mojom::UserActionRecorder> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SERVICES_METRICS_SETTINGS_USER_ACTION_TRACKER_H_
