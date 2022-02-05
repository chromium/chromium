// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_URL_HANDLERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_URL_HANDLERS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace base {
class Value;
}

namespace web_app {
class WebAppRegistrar;
}  // namespace web_app

namespace settings {

class UrlHandlersHandlerTest;

class UrlHandlersHandler : public SettingsPageUIHandler {
 public:
  UrlHandlersHandler(PrefService* local_state, Profile* profile);
  ~UrlHandlersHandler() override;
  UrlHandlersHandler(const UrlHandlersHandler&) = delete;
  UrlHandlersHandler& operator=(const UrlHandlersHandler&) = delete;

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 protected:
  // Protected for tests.
  UrlHandlersHandler(PrefService* local_state,
                     Profile* profile,
                     web_app::WebAppRegistrar* web_app_registrar);

 private:
  friend class ::settings::UrlHandlersHandlerTest;

  // Handles calls from WebUI to update the model data of URL handlers settings.
  // Takes no args.
  void HandleGetUrlHandlers(base::Value::ConstListView args);

  // Handles calls from WebUI to reset the user's saved choice for one or more
  // URL handler entries.
  // When reset, the choice becomes kNone and the timestamp is updated.
  // |args| is a list of [app_id, origin_key, has_origin_wildcard, url_path].
  void HandleResetUrlHandlerSavedChoice(base::Value::ConstListView args);

  // Reads and formats data from UrlHandlerPrefs then sends it to the WebUI
  // frontend.
  void UpdateModel();

  // Listens to relevant prefs changes and updates WebUI with new data.
  void OnUrlHandlersLocalStatePrefChanged();

  // Helper functions that read and format UrlHandlerPrefs data for settings
  // page in WebUI.
  base::Value GetEnabledHandlersList();
  base::Value GetDisabledHandlersList();

  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;
  const raw_ptr<PrefService> local_state_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<web_app::WebAppRegistrar> web_app_registrar_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_URL_HANDLERS_HANDLER_H_
