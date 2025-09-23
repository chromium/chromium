// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_uninstall_dialog_user_options.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
              BUILDFLAG(IS_CHROMEOS));

class GURL;
class Profile;
class Browser;

namespace base {
class FilePath;
class TimeTicks;
}  // namespace base

namespace content {
class WebContents;
}

namespace webapps {
class MlInstallOperationTracker;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class IsolatedWebAppInstallerCoordinator;
class WebAppScreenshotFetcher;
struct WebAppInstallInfo;

// Callback used to indicate whether a user has accepted the installation of a
// web app. The boolean parameter is true when the user accepts the dialog. The
// WebAppInstallInfo parameter contains the information about the app,
// possibly modified by the user.
using AppInstallationAcceptanceCallback =
    base::OnceCallback<void(bool, std::unique_ptr<WebAppInstallInfo>)>;

// Shows the Create Shortcut confirmation dialog.
//
// |web_app_info| is the WebAppInstallInfo being converted into an app.
void ShowCreateShortcutDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback);

// When an app changes its icon or name, that is considered an app identity
// change which (for some types of apps) needs confirmation from the user.
// This function shows that confirmation dialog. |app_id| is the unique id of
// the app that is updating and |title_change| and |icon_change| specify which
// piece of information is changing. Can be one or the other, or both (but
// both cannot be |false|). |old_title| and |new_title|, as well as |old_icon|
// and |new_icon| show the 'before' and 'after' values. A response is sent
// back via the |callback|.
void ShowWebAppIdentityUpdateDialog(const std::string& app_id,
                                    bool title_change,
                                    bool icon_change,
                                    const std::u16string& old_title,
                                    const std::u16string& new_title,
                                    const SkBitmap& old_icon,
                                    const SkBitmap& new_icon,
                                    content::WebContents* web_contents,
                                    AppIdentityDialogCallback callback);

// Shows the an app update review dialog that always shows the name, icon, and
// start url of the before and after states of the app. The user can accept,
// ignore, or uninstall the app. This won't apply any of those changes, the
// response is sent back via the `callback`, and the caller is expected to
// facilitate those actual operations.
// See the `WebAppIdentityUpdateResult` type for the possible responses.
void ShowWebAppReviewUpdateDialog(const webapps::AppId& app_id,
                                  const WebAppIdentityUpdate& update,
                                  Browser* browser,
                                  base::TimeTicks start_time,
                                  UpdateReviewDialogCallback callback);

// Shows the web app uninstallation dialog on a page whenever user has decided
// to uninstall an installed dPWA from a variety of OS surfaces and chrome.
void ShowWebAppUninstallDialog(
    Profile* profile,
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source,
    gfx::NativeWindow parent,
    IconMetadataFromDisk icon_metadata,
    UninstallDialogCallback uninstall_dialog_result_callback);

// Callback used to indicate whether a user has accepted the launch of a
// web app. The |allowed| is true when the user allows the app to launch.
// |remember_user_choice| is true if the user wants to persist the decision.
using WebAppLaunchAcceptanceCallback =
    base::OnceCallback<void(bool allowed, bool remember_user_choice)>;

// Shows the pre-launch dialog for protocol handling PWA launch. The user can
// allow or block the launch.
void ShowWebAppProtocolLaunchDialog(
    const GURL& url,
    Profile* profile,
    const webapps::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback);

// Shows the pre-launch dialog for a file handling PWA launch. The user can
// allow or block the launch.
void ShowWebAppFileLaunchDialog(const std::vector<base::FilePath>& file_paths,
                                Profile* profile,
                                const webapps::AppId& app_id,
                                WebAppLaunchAcceptanceCallback close_callback);
// Sets whether |ShowWebAppDialog| should accept immediately without any
// user interaction. |auto_open_in_window| sets whether the open in window
// checkbox is checked.
void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,
                                         bool auto_open_in_window);

// Sets an override title for the Create Shortcut confirmation view.
void SetOverrideTitleForTesting(const char* title_to_use);

// Describes the state of in-product-help being shown to the user.
enum class PwaInProductHelpState {
  // The in-product-help bubble was shown.
  kShown,
  // The in-product-help bubble was not shown.
  kNotShown
};

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogAppTitle);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogIconView);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSimpleInstallDialogOriginLabel);

// Shows the PWA installation confirmation bubble anchored off the PWA install
// icon in the omnibox.
//
// |web_app_info| is the WebAppInstallInfo to be installed.
// |callback| is called when install bubble closed.
// |iph_state| records whether PWA install iph is shown before Install bubble is
// shown.
void ShowSimpleInstallDialogForWebApps(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown,
    bool show_initiating_origin = false);

// Shows the PWA install dialog for apps that are not installable, AKA, DIY
// apps.
void ShowDiyAppInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDetailedInstallDialogImageContainer);

// Shows the Web App detailed install dialog.
// The dialog shows app's detailed information including screenshots. Users then
// confirm or cancel install in this dialog.
void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
    AppInstallationAcceptanceCallback callback,
    base::WeakPtr<WebAppScreenshotFetcher> screenshot_fetcher,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Sets whether |ShowSimpleInstallDialogForWebApps| should accept immediately
// without any user interaction.
base::AutoReset<bool> SetAutoAcceptPWAInstallConfirmationForTesting();

// Sets whether |ShowSimpleInstallDialogForWebApps| should decline immediately
// without any user interaction.
base::AutoReset<bool> SetAutoDeclinePWAInstallConfirmationForTesting();

// Sets whether |ShowDiyInstallDialogForWebApps| should accept immediately
// without any user interaction.
void SetAutoAcceptDiyAppsInstallDialogForTesting(bool auto_accept);

// Sets whether the bubble should close when it is not in an active window
// during testing.
base::AutoReset<bool> SetDontCloseOnDeactivateForTesting();

// Shows the Isolated Web App manual install wizard.
IsolatedWebAppInstallerCoordinator* LaunchIsolatedWebAppInstaller(
    Profile* profile,
    const base::FilePath& bundle_path,
    base::OnceClosure on_closed_callback);

void FocusIsolatedWebAppInstaller(
    IsolatedWebAppInstallerCoordinator* coordinator);

void PostCallbackOnBrowserActivation(
    const Browser* browser,
    ui::ElementIdentifier id,
    base::OnceCallback<void(bool)> view_and_element_activated_callback);

// Callback used by the Web Install API to indicate whether the user has
// accepted the launch of a web app.
using WebInstallAppLaunchAcceptanceCallback =
    base::OnceCallback<void(bool accepted)>;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebInstallLaunchDialogAppName);

// Shows a web app launch dialog for `app_id`. Used by the Web Install API. The
// dialog contains the app short name and icon, just like the intent picker. The
// user can accept or cancel the launch. A response is sent via `callback` so
// the service implementation can resolve itself based on the user
// interaction.
void ShowWebInstallAppLaunchDialog(
    content::WebContents* web_contents,
    const webapps::AppId& app_id,
    Profile* profile,
    std::string app_name,
    const SkBitmap& icon,
    WebInstallAppLaunchAcceptanceCallback callback);

// Sets whether |ShowWebInstallAppLaunchDialog| should accept immediately.
base::AutoReset<bool> SetAutoAcceptWebInstallLaunchDialogForTesting();

// Shows the install not supported dialog for web apps. This dialog is
// displayed when the user tries to install a web app in an unsupported
// environment, such as Incognito or Guest mode. The |callback| is called
// when the dialog is closed.
void ShowInstallNotSupportedDialog(content::WebContents* web_contents,
                                   Profile* profile,
                                   base::OnceClosure callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_DIALOGS_H_
