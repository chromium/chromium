// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_

#include <map>
#include <memory>

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/win/conflicts/installed_applications.h"

class RegistryKeyWatcher;

namespace settings {

// Incompatible Applications settings page UI handler.
class IncompatibleApplicationsHandler : public SettingsPageUIHandler {
 public:
  IncompatibleApplicationsHandler();

  IncompatibleApplicationsHandler(const IncompatibleApplicationsHandler&) =
      delete;
  IncompatibleApplicationsHandler& operator=(
      const IncompatibleApplicationsHandler&) = delete;

  ~IncompatibleApplicationsHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Sends the list of incompatible applications to the caller via a promise.
  void HandleRequestIncompatibleApplicationsList(const base::Value::List& args);

  // Initiates the uninstallation of the application passed using |args|.
  void HandleStartApplicationUninstallation(const base::Value::List& args);

  void HandleGetSubtitlePluralString(const base::Value::List& args);
  void HandleGetSubtitleNoAdminRightsPluralString(
      const base::Value::List& args);
  void HandleGetListTitlePluralString(const base::Value::List& args);
  void GetPluralString(int id, const base::Value::List& args);

  // Callback for the registry key watchers.
  void OnApplicationRemoved(
      const InstalledApplications::ApplicationInfo& application);

  // Container for the watchers.
  std::map<InstalledApplications::ApplicationInfo,
           std::unique_ptr<RegistryKeyWatcher>>
      registry_key_watchers_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_INCOMPATIBLE_APPLICATIONS_HANDLER_WIN_H_
