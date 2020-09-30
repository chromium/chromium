// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SETTINGS_USER_ACTION_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SETTINGS_USER_ACTION_TRACKER_H_

#include <memory>

#include "chrome/browser/ui/webui/settings/chromeos/search/per_session_settings_user_action_tracker.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/user_action_recorder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace settings {

// Records user actions within Settings. Utilizes a per session tracker that
// measures the user's effort required to change a setting. Eventually uses
// a per section tracker to record metrics in each section.
class SettingsUserActionTracker : public mojom::UserActionRecorder {
 public:
  SettingsUserActionTracker();
  SettingsUserActionTracker(const SettingsUserActionTracker& other) = delete;
  SettingsUserActionTracker& operator=(const SettingsUserActionTracker& other) =
      delete;
  ~SettingsUserActionTracker() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::UserActionRecorder> pending_receiver);

 private:
  // mojom::UserActionRecorder:
  void RecordPageFocus() override;
  void RecordPageBlur() override;
  void RecordClick() override;
  void RecordNavigation() override;
  void RecordSearch() override;
  void RecordSettingChange() override;

  void EndCurrentSession();
  void OnBindingDisconnected();

  std::unique_ptr<PerSessionSettingsUserActionTracker> per_session_tracker_;

  mojo::Receiver<mojom::UserActionRecorder> receiver_{this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SETTINGS_USER_ACTION_TRACKER_H_
