// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

class GURL;

struct WebApplicationInfo;

namespace web_app {

class WebAppInstallFinalizer;
enum class FileHandlerUpdateAction;

// This class helps `WebAppInstallFinalizer` manage file handling permissions
// for PWAs. It keeps the web app database and OS registrations in sync when
// permissions (content settings) change, and resets granted permissions after
// certain install/update/uninstall events.
class FileHandlersPermissionHelper : public content_settings::Observer,
                                     public AppRegistrarObserver {
 public:
  explicit FileHandlersPermissionHelper(WebAppInstallFinalizer* finalizer);
  ~FileHandlersPermissionHelper() override;
  FileHandlersPermissionHelper(const FileHandlersPermissionHelper& other) =
      delete;
  FileHandlersPermissionHelper& operator=(
      const FileHandlersPermissionHelper& other) = delete;

  // To be called when an app is going to be installed with `web_app_info`.
  void WillInstallApp(const WebApplicationInfo& web_app_info);

  // To be called before the app corresponding to `app_id` is updated with
  // `web_app_info` changes. Checks whether OS registered file handlers need to
  // update, taking into account permission settings, as file handlers should be
  // unregistered when the permission has been denied. Also, downgrades granted
  // file handling permissions if file handlers have changed. Returns whether
  // the OS file handling registrations need to be updated.
  FileHandlerUpdateAction WillUpdateApp(const AppId app_id,
                                        const WebApplicationInfo& web_app_info);

  // Checks if file handling permission is blocked in settings.
  bool IsPermissionBlocked(const GURL& scope);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // AppRegistrarObserver:
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;

 private:
  // Resets the FILE_HANDLING content setting permission if `web_app_info` is
  // asking for more file handling types than were previously granted to the
  // app's origin. Returns the new content setting, which will be either
  // unchanged, or will have switched from ALLOW to ASK. If the previous setting
  // was BLOCK, no change is made.
  ContentSetting MaybeResetPermission(const WebApplicationInfo& web_app_info);

  // Update file handler state in database and OS settings as per the setting
  // reported by the permission manager, for all apps where the scope matches
  // `pattern`.
  void UpdateAppsMatchingPattern(const ContentSettingsPattern& pattern);

  WebAppInstallFinalizer* finalizer_;

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observer_{this};
  base::ScopedObservation<WebAppRegistrar, AppRegistrarObserver>
      app_registrar_observer_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FILE_HANDLERS_PERMISSION_HELPER_H_
