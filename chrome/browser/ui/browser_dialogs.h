// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/web_applications/web_app_id.h"
#endif

class Browser;
class GURL;
class LoginHandler;
class Profile;
struct WebAppInstallInfo;

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SettingsOverriddenDialogController;
#endif

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace net {
class AuthChallengeInfo;
}

namespace permissions {
class ChooserController;
enum class PermissionAction;
}  // namespace permissions

namespace safe_browsing {
class ChromeCleanerController;
class ChromeCleanerDialogController;
class ChromeCleanerRebootDialogController;
class SettingsResetPromptController;
}  // namespace safe_browsing

namespace task_manager {
class TaskManagerTableModel;
}

namespace ui {
class WebDialogDelegate;
struct SelectedFileInfo;
}  // namespace ui

namespace chrome {

// Shows or hides the Task Manager. |browser| can be NULL when called from Ash.
// Returns a pointer to the underlying TableModel, which can be ignored, or used
// for testing.
task_manager::TaskManagerTableModel* ShowTaskManager(Browser* browser);
void HideTaskManager();

// Creates and shows an HTML dialog with the given delegate and context.
// The window is automatically destroyed when it is closed.
// Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate,
                                bool show = true);

// Show `dialog_model` as a modal dialog to `browser`.
void ShowBrowserModal(Browser* browser,
                      std::unique_ptr<ui::DialogModel> dialog_model);

// Show `dialog_model` as a bubble anchored to `anchor_element` in `browser`.
// `anchor_element` must refer to an element currently present in `browser`.
// TODO(pbos): Make utility functions for querying whether an anchor_element is
// present in `browser` or `browser_window` and then refer to those here so that
// a call site can provide fallback options for `anchor_element`.
void ShowBubble(Browser* browser,
                ui::ElementIdentifier anchor_element,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the create chrome app shortcut dialog box.
// |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    base::OnceCallback<void(bool /* created */)> close_callback);

// Shows the create chrome app shortcut dialog box. Same as above but for a
// WebApp instead of an Extension. |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const std::string& web_app_id,
    base::OnceCallback<void(bool /* created */)> close_callback);

#if PAIR_BLUETOOTH_ON_DEMAND()
// Shows the dialog to request the Bluetooth credentials for the device
// identified by |device_identifier|. |device_identifier| is the most
// appropriate string to display to the user for device identification
// (e.g. name, MAC address).
void ShowBluetoothDeviceCredentialsDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    content::BluetoothDelegate::PairPromptCallback close_callback);

// Show a user prompt for pairing a Bluetooth device. |device_identifier|
// is the most appropriate string to display for device identification
// (e.g. name, MAC address). The |pin| is displayed (if specified),
// so the user can confirm a matching value is displayed on the device.
void ShowBluetoothDevicePairConfirmDialog(
    content::WebContents* web_contents,
    const std::u16string& device_identifier,
    const absl::optional<std::u16string> pin,
    content::BluetoothDelegate::PairPromptCallback close_callback);
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

// Callback used to indicate whether a user has accepted the installation of a
// web app. The boolean parameter is true when the user accepts the dialog. The
// WebAppInstallInfo parameter contains the information about the app,
// possibly modified by the user.
using AppInstallationAcceptanceCallback =
    base::OnceCallback<void(bool, std::unique_ptr<WebAppInstallInfo>)>;

// Shows the Web App install bubble.
//
// |web_app_info| is the WebAppInstallInfo being converted into an app.
// |web_app_info.app_url| should contain a start url from a web app manifest
// (for a Desktop PWA), or the current url (when creating a shortcut app).
void ShowWebAppInstallDialog(content::WebContents* web_contents,
                             std::unique_ptr<WebAppInstallInfo> web_app_info,
                             AppInstallationAcceptanceCallback callback);

// When an app changes its icon or name, that is considered an app identity
// change which (for some types of apps) needs confirmation from the user.
// This function shows that confirmation dialog. |app_id| is the unique id of
// the app that is updating and |title_change| and |icon_change| specify which
// piece of information is changing. Can be one or the other, or both (but
// both cannot be |false|). |old_title| and |new_title|, as well as |old_icon|
// and |new_icon| show the 'before' and 'after' values. A response is sent
// back via the |callback|.
void ShowWebAppIdentityUpdateDialog(
    const std::string& app_id,
    bool title_change,
    bool icon_change,
    const std::u16string& old_title,
    const std::u16string& new_title,
    const SkBitmap& old_icon,
    const SkBitmap& new_icon,
    content::WebContents* web_contents,
    web_app::AppIdentityDialogCallback callback);

// Sets whether |ShowWebAppIdentityUpdateDialog| should accept immediately
// without any user interaction.
void SetAutoAcceptAppIdentityUpdateForTesting(bool auto_accept);

#if !BUILDFLAG(IS_ANDROID)
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
    const web_app::AppId& app_id,
    WebAppLaunchAcceptanceCallback close_callback);

// Shows the pre-launch dialog for a file handling PWA launch. The user can
// allow or block the launch.
void ShowWebAppFileLaunchDialog(const std::vector<base::FilePath>& file_paths,
                                Profile* profile,
                                const web_app::AppId& app_id,
                                WebAppLaunchAcceptanceCallback close_callback);
#endif  // !BUILDFLAG(IS_ANDROID)

// Sets whether |ShowWebAppDialog| should accept immediately without any
// user interaction. |auto_open_in_window| sets whether the open in window
// checkbox is checked.
void SetAutoAcceptWebAppDialogForTesting(bool auto_accept,
                                         bool auto_open_in_window);

// Describes the state of in-product-help being shown to the user.
enum class PwaInProductHelpState {
  // The in-product-help bubble was shown.
  kShown,
  // The in-product-help bubble was not shown.
  kNotShown
};

// Shows the PWA installation confirmation bubble anchored off the PWA install
// icon in the omnibox.
//
// |web_app_info| is the WebAppInstallInfo to be installed.
// |callback| is called when install bubble closed.
// |iph_state| records whether PWA install iph is shown before Install bubble is
// shown.
void ShowPWAInstallBubble(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Shows the Web App detailed install dialog.
// The dialog shows app's detailed information including screenshots. Users then
// confirm or cancel install in this dialog.
void ShowWebAppDetailedInstallDialog(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    AppInstallationAcceptanceCallback callback,
    const std::vector<SkBitmap>& screenshots,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Sets whether |ShowPWAInstallBubble| should accept immediately without any
// user interaction.
void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept);

#if BUILDFLAG(IS_MAC)

// Bridging methods that show/hide the toolkit-views based Task Manager on Mac.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser);
void HideTaskManagerViews();

#endif  // BUILDFLAG(IS_MAC)

#if defined(TOOLKIT_VIEWS)

// Creates a toolkit-views based LoginHandler (e.g. HTTP-Auth dialog).
std::unique_ptr<LoginHandler> CreateLoginHandlerViews(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback);

#endif  // TOOLKIT_VIEWS

#if BUILDFLAG(IS_WIN)

// Shows the settings reset prompt dialog asking the user if they want to reset
// some of their settings.
void ShowSettingsResetPrompt(
    Browser* browser,
    safe_browsing::SettingsResetPromptController* controller);

// Shows the Chrome Cleanup dialog asking the user if they want to clean their
// system from unwanted software. This is called when unwanted software has been
// detected on the system.
void ShowChromeCleanerPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerDialogController* dialog_controller,
    safe_browsing::ChromeCleanerController* cleaner_controller);

// Shows the Chrome Cleanup reboot dialog asking the user if they want to
// restart their computer once a cleanup has finished. This is called when the
// Chrome Cleanup ends in a reboot required state.
void ShowChromeCleanerRebootPrompt(
    Browser* browser,
    safe_browsing::ChromeCleanerRebootDialogController* dialog_controller);

#endif  // BUILDFLAG(IS_WIN)

// Displays a dialog to notify the user that the extension installation is
// blocked due to policy. It also show additional information from administrator
// if it exists.
void ShowExtensionInstallBlockedDialog(
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
// The type of action that the ExtensionInstalledBlockedByParentDialog
// is being shown in reaction to.
enum class ExtensionInstalledBlockedByParentDialogAction {
  kAdd,     // The user attempted to add the extension.
  kEnable,  // The user attempted to enable the extension.
};

// Displays a dialog to notify the user that the extension installation is
// blocked by a parent
void ShowExtensionInstallBlockedByParentDialog(
    ExtensionInstalledBlockedByParentDialogAction action,
    const extensions::Extension* extension,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)

// TODO(crbug.com/1324288): Move extensions dialogs to
// c/b/ui/extensions/extensions_dialogs.h
// TODO(devlin): Put more extension-y bits in this block - currently they're
// unguarded.
#if BUILDFLAG(ENABLE_EXTENSIONS)
// Shows the dialog indicating that an extension has overridden a setting.
void ShowExtensionSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    Browser* browser);

// Modal dialog shown to Enhanced Safe Browsing users before the extension
// install dialog if the extension is not included in the Safe Browsing CRX
// allowlist.
//
// `callback` will be invoked with `true` if the user accepts or `false` if the
// user cancels the dialog.
void ShowExtensionInstallFrictionDialog(
    content::WebContents* contents,
    base::OnceCallback<void(bool)> callback);

#endif

// Returns a OnceClosure that client code can call to close the device chooser.
// This OnceClosure references the actual dialog as a WeakPtr, so it's safe to
// call at any point.
#if defined(TOOLKIT_VIEWS)
base::OnceClosure ShowDeviceChooserDialog(
    content::RenderFrameHost* owner,
    std::unique_ptr<permissions::ChooserController> controller);
bool IsDeviceChooserShowingForTesting(Browser* browser);
#endif

// Show the prompt to set a window name for browser's window, optionally with
// the given context.
void ShowWindowNamePrompt(Browser* browser);
std::unique_ptr<ui::DialogModel> CreateWindowNamePromptDialogModelForTesting(
    Browser* browser);

// Callback used to indicate whether Direct Sockets connection dialog is
// accepted or not. If accepted, the remote address and port number are
// provided.
using OnProceedCallback = base::OnceCallback<
    void(bool accepted, const std::string& address, const std::string& port)>;

}  // namespace chrome

void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
