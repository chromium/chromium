// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#endif

struct WebAppInstallInfo;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// |app_id| may be empty on failure.
using OnceInstallCallback =
    base::OnceCallback<void(const AppId& app_id,
                            webapps::InstallResultCode code)>;
using OnceUninstallCallback =
    base::OnceCallback<void(const AppId& app_id, bool uninstalled)>;

// Callback used to indicate whether a user has accepted the installation of a
// web app.
using WebAppInstallationAcceptanceCallback =
    base::OnceCallback<void(bool user_accepted,
                            std::unique_ptr<WebAppInstallInfo>)>;

// Callback to show the WebApp installation confirmation bubble in UI.
// |web_app_info| is the WebAppInstallInfo to be installed.
using WebAppInstallDialogCallback = base::OnceCallback<void(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallationAcceptanceCallback acceptance_callback)>;

// See related ExternalInstallOptions struct and
// ConvertExternalInstallOptionsToParams function.
struct WebAppInstallParams {
  WebAppInstallParams();
  ~WebAppInstallParams();
  WebAppInstallParams(const WebAppInstallParams&);

  // Whether the app should be reinstalled even if it is already installed.
  bool force_reinstall = false;

  // See `WebAppInstallTask::ApplyParamsToWebAppInstallInfo`
  absl::optional<mojom::UserDisplayMode> user_display_mode = absl::nullopt;

  // URL to be used as start_url if manifest is unavailable.
  GURL fallback_start_url;

  // Setting this field will force the webapp to have a manifest id, which
  // will result in a different AppId than if it isn't set. Currently here
  // to support forwards compatibility with future sync entities..
  absl::optional<std::string> override_manifest_id;

  // App name to be used if manifest is unavailable.
  absl::optional<std::u16string> fallback_app_name;

  bool locally_installed = true;

  // If true, OsIntegrationManager::InstallOsHooks won't be called at all,
  // meaning that all other OS Hooks related parameters will be ignored.
  bool bypass_os_hooks = false;

  // These OS shortcut fields can't be true if |locally_installed| is false.
  // They only have an effect when |bypass_os_hooks| is false.
  bool add_to_applications_menu = true;
  bool add_to_desktop = true;
  bool add_to_quick_launch_bar = true;

  // These have no effect outside of Chrome OS.
  bool add_to_search = true;
  bool add_to_management = true;
  bool is_disabled = false;
  bool handles_file_open_intents = true;

  bool bypass_service_worker_check = false;
  bool require_manifest = false;

  // Used only by ExternallyManagedInstallCommand.
  // Has the same meaning as WebAppInstallFlow::kCreateShortcut
  bool install_as_shortcut = false;

  std::vector<std::string> additional_search_terms;

  absl::optional<std::string> launch_query_params;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<ash::SystemWebAppType> system_app_type;
#endif

  bool oem_installed = false;

  // The install URL for the app. This does not always need to be
  // populated (especially for user installed or sync installed apps)
  // in which case the URL will not be written to the web_app DB.
  GURL install_url;
};

// The different UI flows that exist for creating a web app.
enum class WebAppInstallFlow {
  // TODO(crbug.com/1216457): This should be removed by adding all known flows
  // to this enum.
  kUnknown,
  // The 'Create Shortcut' flow for adding the current page as a shortcut app.
  kCreateShortcut,
  // The 'Install Site' flow for installing the current site with an app
  // experience determined by the site.
  kInstallSite,
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_
