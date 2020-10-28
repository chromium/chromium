// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SWITCH_ACCESS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SWITCH_ACCESS_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/events/event_handler.h"

namespace base {
class ListValue;
}

class PrefService;

namespace chromeos {
namespace settings {

// Settings handler for the switch access subpage.
class SwitchAccessHandler : public ::settings::SettingsPageUIHandler,
                            public ui::EventHandler {
 public:
  explicit SwitchAccessHandler(PrefService* prefs);

  ~SwitchAccessHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ui::EventHandler overrides.
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void HandleRefreshAssignmentsFromPrefs(const base::ListValue* args);
  void HandleNotifySwitchAccessActionAssignmentDialogAttached(
      const base::ListValue* args);
  void HandleNotifySwitchAccessActionAssignmentDialogDetached(
      const base::ListValue* args);
  void OnSwitchAccessAssignmentsUpdated();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  PrefService* prefs_;
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SWITCH_ACCESS_HANDLER_H_
