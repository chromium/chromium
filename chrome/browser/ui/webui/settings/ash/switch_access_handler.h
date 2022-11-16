// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SWITCH_ACCESS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SWITCH_ACCESS_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/events/event_handler.h"

class PrefService;

namespace ash::settings {

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
  void HandleRefreshAssignmentsFromPrefs(const base::Value::List& args);
  void HandleNotifySwitchAccessActionAssignmentPaneActive(
      const base::Value::List& args);
  void HandleNotifySwitchAccessActionAssignmentPaneInactive(
      const base::Value::List& args);
  void OnSwitchAccessAssignmentsUpdated();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  PrefService* prefs_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SWITCH_ACCESS_HANDLER_H_
