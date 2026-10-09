// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_DEVICE_TAB_VISIBILITY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_DEVICE_TAB_VISIBILITY_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace settings {

// WebUI message handler for the `chrome://settings/deviceTabVisibility`
// subpage. Provides the list of synced foreign sessions and persists local
// per-device tab visibility preferences.
class DeviceTabVisibilityHandler : public SettingsPageUIHandler {
 public:
  explicit DeviceTabVisibilityHandler(Profile* profile);

  DeviceTabVisibilityHandler(const DeviceTabVisibilityHandler&) = delete;
  DeviceTabVisibilityHandler& operator=(const DeviceTabVisibilityHandler&) =
      delete;

  ~DeviceTabVisibilityHandler() override;

  // `SettingsPageUIHandler`:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Builds the list of foreign device entries for WebUI consumption.
  base::ListValue GetDeviceTabVisibilityList() const;

  // Handles the `"getDeviceTabVisibilityList"` message.
  void HandleGetDeviceTabVisibilityList(const base::ListValue& args);

  // Handles the `"setDeviceTabVisibility"` message with arguments
  // `[callback_id, session_tag, visible]`.
  void HandleSetDeviceTabVisibility(const base::ListValue& args);

  // Notifies the WebUI listener when synced foreign sessions or local
  // visibility preferences change.
  void OnForeignSessionsChanged();

  const raw_ref<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription foreign_session_updated_subscription_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_DEVICE_TAB_VISIBILITY_HANDLER_H_
