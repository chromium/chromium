// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SWITCH_ACCESS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SWITCH_ACCESS_HANDLER_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/events/event_handler.h"

class PrefService;

namespace ash::settings {

// Settings handler for the switch access subpage.
class SwitchAccessHandler : public content::WebUIMessageHandler,
                            public ui::EventHandler {
 public:
  explicit SwitchAccessHandler(PrefService* prefs);

  ~SwitchAccessHandler() override;

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ui::EventHandler overrides.
  void OnKeyEvent(ui::KeyEvent* event) override;

 protected:
  base::OnceClosure on_pre_target_handler_added_for_testing_ =
      base::DoNothing();
  bool skip_pre_target_handler_for_testing_ = false;

 private:
  friend class SwitchAccessHandlerTest;

  void AddPreTargetHandler();
  void HandleRefreshAssignmentsFromPrefs(const base::ListValue& args);
  void HandleNotifySwitchAccessActionAssignmentPaneActive(
      const base::ListValue& args);
  void HandleNotifySwitchAccessActionAssignmentPaneInactive(
      const base::ListValue& args);
  void OnSwitchAccessAssignmentsUpdated();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  raw_ptr<PrefService> prefs_;

  bool action_assignment_pane_active_ = false;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_SWITCH_ACCESS_HANDLER_H_
