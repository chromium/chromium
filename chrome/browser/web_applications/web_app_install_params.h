// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/system_apps/public/system_web_app_type.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

struct WebAppInstallInfo;

// |app_id| may be empty on failure.
using OnceInstallCallback =
    base::OnceCallback<void(const webapps::AppId& app_id,
                            webapps::InstallResultCode code)>;
using OnceUninstallCallback =
    base::OnceCallback<void(const webapps::AppId& app_id, bool uninstalled)>;

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
  std::optional<mojom::UserDisplayMode> user_display_mode = std::nullopt;

  // URL to be used as start_url if manifest is unavailable.
  GURL fallback_start_url;

  // App name to be used if manifest is unavailable.
  std::optional<std::u16string> fallback_app_name;

  proto::InstallState install_state =
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;

  // These are required to be false if `install_state` is not
  // proto::INSTALLED_WITH_OS_INTEGRATION.
  bool add_to_applications_menu = true;
  bool add_to_desktop = true;
  bool add_to_quick_launch_bar = true;

  // These have no effect outside of Chrome OS.
  bool add_to_search = true;
  bool add_to_management = true;
  bool is_disabled = false;
  bool handles_file_open_intents = true;

  bool require_manifest = false;

  // Used only by ExternallyManagedInstallCommand.
  // Has the same meaning as WebAppInstallFlow::kCreateShortcut
  // TODO(crbug.com/339718933): This is a deprecated feature. To install a site
  // as an app, install it as a 'diy' app instead, or use
  // shortcuts::CreateShortcutForWebContents().
  bool install_as_shortcut = false;

  std::vector<std::string> additional_search_terms;

  std::optional<std::string> launch_query_params;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::optional<ash::SystemWebAppType> system_app_type;
#endif

  bool oem_installed = false;

  // The install URL for the app. This does not always need to be
  // populated (especially for user installed or sync installed apps)
  // in which case the URL will not be written to the web_app DB.
  GURL install_url;

  // If true, do not validate origin associations as part of the install even if
  // app has valid scope_extensions.
  bool skip_origin_association_validation = false;
};

// The different UI flows that exist for creating a web app.
enum class WebAppInstallFlow {
  // TODO(crbug.com/40184819): This should be removed by adding all known flows
  // to this enum.
  kUnknown,
  // The 'Create Shortcut' flow for adding the current page as a shortcut app.
  // TODO(crbug.com/339718933): This is a deprecated feature. To install a site
  // as an app, install it as a 'diy' app instead (currently by using
  // kInstallSite). Or use shortcuts::CreateShortcutForWebContents().
  kCreateShortcut,
  // The 'Install Site' flow for installing the current site with an app
  // experience determined by the site.
  kInstallSite,
};

enum class FallbackBehavior {
  // Installation will use the crafted manifest, and error if the manifest is
  // not installable.
  kCraftedManifestOnly,
  // Installation will use whatever is available - if the site is installable
  // then crafted app UX will be used, and if not then DIY WebApp UX will be
  // used. See go/dpwa-universal-install.
  kUseFallbackInfoWhenNotInstallable,
  // Installation uses the legacy 'create shortcut' flow, which uses the crafted
  // manifest if possible, and otherwise fallback information (which has an
  // empty
  // 'scope()', so IsShortcut() returns true).
  kAllowFallbackDataAlways,
};

std::ostream& operator<<(std::ostream& os, FallbackBehavior state);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_PARAMS_H_
