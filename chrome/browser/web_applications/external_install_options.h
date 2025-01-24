// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_INSTALL_OPTIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_INSTALL_OPTIONS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#endif

namespace web_app {

using WebAppInstallInfoFactory =
    base::RepeatingCallback<std::unique_ptr<WebAppInstallInfo>()>;

enum class ExternalInstallSource;

enum class PlaceholderResolutionBehavior {
  // Waits for all running app instances being closed before the placeholder is
  // resolved.
  kWaitForAppWindowsClosed,
  // Closes all running app instances, resolves the placeholder and relaunches
  // the app in a separate window.
  kCloseAndRelaunch,
  // Closes all running app instances and resolves the placeholder. After the
  // placeholder is resolved, the app is not relaunched.
  kClose
};

struct ExternalInstallOptions {
  ExternalInstallOptions(
      const GURL& install_url,
      std::optional<mojom::UserDisplayMode> user_display_mode,
      ExternalInstallSource install_source);

  ~ExternalInstallOptions();
  ExternalInstallOptions(const ExternalInstallOptions& other);
  ExternalInstallOptions(ExternalInstallOptions&& other);
  ExternalInstallOptions& operator=(const ExternalInstallOptions& other);

  bool operator==(const ExternalInstallOptions& other) const;

  base::Value AsDebugValue() const;

  GURL install_url;

  std::optional<mojom::UserDisplayMode> user_display_mode;

  ExternalInstallSource install_source;

  // App name to use for placeholder apps or web apps that have no name in
  // their manifest.
  std::optional<std::string> fallback_app_name;

  // App name that replaces the app's real name.
  // override_name takes precedent over fallback_app_name in case both are
  // present.
  std::optional<std::string> override_name;

  // URL of an icon that replaces the app's real icons.
  std::optional<GURL> override_icon_url;

  // If true, a shortcut is added to the Applications folder on macOS, and Start
  // Menu on Linux and Windows and launcher on Chrome OS. If false, we skip
  // adding a shortcut to desktop as well, regardless of the value of
  // |add_to_desktop|.
  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  bool add_to_applications_menu = true;

  // If true, a shortcut is added to the desktop on Linux and Windows. Has no
  // effect on macOS and Chrome OS.
  bool add_to_desktop = true;

  // If true, a shortcut is added to the "quick launch bar" of the OS: the Shelf
  // for Chrome OS, the Dock for macOS, and the Quick Launch Bar or Taskbar on
  // Windows. Currently this only works on Chrome OS.
  bool add_to_quick_launch_bar = true;

  // If true, the app can be searched for on Chrome OS. Has no effect on other
  // platforms.
  bool add_to_search = true;

  // If true, the app is shown in App Management on Chrome OS. Has no effect on
  // other platforms.
  bool add_to_management = true;

  // If true, the app icon is displayed on Chrome OS with a blocked logo on
  // top, and the user cannot launch the app. Has no effect on other platforms.
  bool is_disabled = false;

  // Whether the app should be reinstalled even if the user has previously
  // uninstalled it. Only applies to preinstalled apps and/or apps that can be
  // uninstalled by the user.
  bool override_previous_user_uninstall = false;

  // Whether the app should only be installed if the user is using Chrome for
  // the first time.
  bool only_for_new_users = false;

  // Whether the app should be omitted for new preinstalls. This is the opposite
  // of |only_for_new_users| option. It can be used for partial deprecation of
  // a preinstalled app: do not preinstall the app for new users but leave
  // existing users unaffected.
  bool only_if_previously_preinstalled = false;

  // Which user types this app should be installed for.
  // See apps::DetermineUserType() for relevant string constants.
  std::vector<std::string> user_type_allowlist;

  // Which feature flag should be enabled to install this app. If the feature
  // is disabled, existing external installs will be removed.
  // See chrome/browser/web_applications/preinstalled_app_install_features.h
  // for available features to gate on.
  std::optional<std::string> gate_on_feature;

  // Which feature flag should be enabled to install this app. If the feature is
  // disabled, existing external installs will not be removed.
  // See chrome/browser/web_applications/preinstalled_app_install_features.h
  // for available features to gate on.
  std::optional<std::string> gate_on_feature_or_installed;

  // Whether this should not be installed for devices that support ARC.
  bool disable_if_arc_supported = false;

  // Whether this should not be installed for tablet devices.
  bool disable_if_tablet_form_factor = false;

  // When set to true this will fail installation with
  // |kNotValidManifestForWebApp| if the |install_url| doesn't have a manifest
  // that passes basic validity checks. This is ignored when |app_info_factory|
  // is used.
  bool require_manifest = false;

  // The web app should be installed as a shortcut, where only limited
  // values from the manifest are used (like theme color) and all extra
  // capabilities are not used (like file handlers).
  // Note: This is different behavior than using the "Create Shortcut..."
  // option in the GUI.
  // TODO(crbug.com/339718933): This is a deprecated feature. To install a site
  // as an app, install it as a 'diy' app instead, or use
  // shortcuts::CreateShortcutForWebContents.
  bool install_as_shortcut = false;

  // Whether the app should be reinstalled even if it is already installed.
  bool force_reinstall = false;

  // Whether we should update the app if the browser's binary milestone number
  // goes from less the milestone specified to greater or equal than the
  // milestone specified. For example, if this value is 89 then we update the
  // app on all browser upgrades from <89 to >=89. The update happens only once.
  std::optional<int> force_reinstall_for_milestone;

  // Defines how to handle running app instances of a placeholder app when a
  // placeholder can be resolved. See `PlaceholderResolutionBehavior`
  // documentation for details.
  PlaceholderResolutionBehavior placeholder_resolution_behavior =
      PlaceholderResolutionBehavior::kClose;

  // Whether a placeholder app should be installed if we fail to retrieve the
  // metadata for the app. A placeholder app uses:
  //  - The default Chrome App icon for the icon
  //  - |url| as the start_url
  //  - |url| as the app name (unless fallback_app_name has been specified)
  bool install_placeholder = false;

  // Optional query parameters to add to the start_url when launching the app.
  std::optional<std::string> launch_query_params;

  // Whether we should load |service_worker_registration_url| after successful
  // installation to allow the site to install its service worker and set up
  // offline caching.
  bool load_and_await_service_worker_registration = true;

  // The URL to use for service worker registration. This is
  // configurable by sites that wish to be able to track install metrics of the
  // install_url separate from the service worker registration step. Defaults to
  // install_url if unset.
  std::optional<GURL> service_worker_registration_url;

  // The time to wait for the service worker registration before it times out.
  // This is currently default at 40 seconds, override this value if more or
  // less time is required.
  base::TimeDelta service_worker_registration_timeout = base::Seconds(40);

  // A list of app_ids that the Web App System should attempt to uninstall and
  // replace with this app (e.g maintain shelf pins, app list positions).
  std::vector<webapps::AppId> uninstall_and_replace;

  // Additional keywords that will be used by the OS when searching for the app.
  // Only affects Chrome OS.
  std::vector<std::string> additional_search_terms;

  // Determines whether |app_info_factory| is used as a fallback or the primary
  // source of app metadata. If true the |install_url| and
  // |service_worker_registration_url| will not be loaded.
  bool only_use_app_info_factory = false;

  // A factory callback that returns a unique_ptr<WebAppInstallInfo> to be used
  // as the app's installation metadata.
  WebAppInstallInfoFactory app_info_factory;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The type of SystemWebApp, if this app is a System Web App.
  std::optional<ash::SystemWebAppType> system_app_type = std::nullopt;
#endif

  // Whether the app was installed by an OEM and should be placed in a special
  // OEM folder in the app launcher. Only used on Chrome OS.
  bool oem_installed = false;

  // Whether this should be installed on devices without a touch screen with
  // stylus support.
  bool disable_if_touchscreen_with_stylus_not_supported = false;

  // Whether the app should show up in file-open intent and picking surfaces.
  bool handles_file_open_intents = false;

  // The app id that's expected to be installed from `install_url`.
  // Does not block installation if the actual app id doesn't match the
  // expectation.
  // Intended to be used for post-install activities like metrics and migration.
  std::optional<webapps::AppId> expected_app_id;

  // Whether the app should be set as the preferred app for its supported links
  // after installation. Note that this has no effect if the app is already
  // installed as the user may have already updated their preference.
  bool is_preferred_app_for_supported_links = false;

  // Whether the app should not be fully installed with os integration
  // (shortcuts in application menu, etc), and instead only installed within
  // Chromium. This sets the installation status to
  // `InstallState::INSTALLED_WITHOUT_OS_INTEGRATION`. This will not
  // downgrade an existing install.
  bool install_without_os_integration = false;

  // Note: All new fields must be added to AsDebugValue() and the == operator.
};

WebAppInstallParams ConvertExternalInstallOptionsToParams(
    const ExternalInstallOptions& install_options);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNAL_INSTALL_OPTIONS_H_
