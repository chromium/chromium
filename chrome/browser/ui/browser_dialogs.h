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
#include "base/optional.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/content_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class ChooserController;
class LoginHandler;
class Profile;
struct WebApplicationInfo;

#if BUILDFLAG(ENABLE_EXTENSIONS)
class SettingsOverriddenDialogController;
#endif

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}

namespace net {
class AuthChallengeInfo;
}

namespace permissions {
enum class PermissionAction;
}

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

// Callback used to indicate whether a user has accepted the installation of a
// web app. The boolean parameter is true when the user accepts the dialog. The
// WebApplicationInfo parameter contains the information about the app,
// possibly modified by the user.
using AppInstallationAcceptanceCallback =
    base::OnceCallback<void(bool, std::unique_ptr<WebApplicationInfo>)>;

// Shows the Web App install bubble.
//
// |web_app_info| is the WebApplicationInfo being converted into an app.
// |web_app_info.app_url| should contain a start url from a web app manifest
// (for a Desktop PWA), or the current url (when creating a shortcut app).
void ShowWebAppInstallDialog(content::WebContents* web_contents,
                             std::unique_ptr<WebApplicationInfo> web_app_info,
                             AppInstallationAcceptanceCallback callback);

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
// |web_app_info| is the WebApplicationInfo to be installed.
// |callback| is called when install bubble closed.
// |iph_state| records whether PWA install iph is shown before Install bubble is
// shown.
void ShowPWAInstallBubble(
    content::WebContents* web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    AppInstallationAcceptanceCallback callback,
    PwaInProductHelpState iph_state = PwaInProductHelpState::kNotShown);

// Sets whether |ShowPWAInstallBubble| should accept immediately without any
// user interaction.
void SetAutoAcceptPWAInstallConfirmationForTesting(bool auto_accept);

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Shows the print job confirmation dialog bubble anchored to the toolbar icon
// for the extension.
// If there's no toolbar icon, shows a modal dialog using
// CreateBrowserModalDialogViews(). Note that this dialog is shown up even if we
// have no |parent| window.
void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const std::string& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)

// Bridging methods that show/hide the toolkit-views based Task Manager on Mac.
task_manager::TaskManagerTableModel* ShowTaskManagerViews(Browser* browser);
void HideTaskManagerViews();

#endif  // OS_MAC

#if defined(TOOLKIT_VIEWS)

// Creates a toolkit-views based LoginHandler (e.g. HTTP-Auth dialog).
std::unique_ptr<LoginHandler> CreateLoginHandlerViews(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback);

#endif  // TOOLKIT_VIEWS

// Values used in the Dialog.Creation UMA metric. Each value represents a
// different type of dialog box.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class DialogIdentifier {
  UNKNOWN = 0,
  TRANSLATE = 1,
  BOOKMARK = 2,
  BOOKMARK_EDITOR = 3,
  DESKTOP_MEDIA_PICKER = 4,
  OUTDATED_UPGRADE = 5,
  ONE_CLICK_SIGNIN = 6,
  PROFILE_SIGNIN_CONFIRMATION = 7,
  HUNG_RENDERER = 8,
  SESSION_CRASHED = 9,
  CONFIRM_BUBBLE = 10,
  UPDATE_RECOMMENDED = 11,
  CRYPTO_PASSWORD = 12,
  SAFE_BROWSING_DOWNLOAD_FEEDBACK = 13,
  FIRST_RUN = 14,
  NETWORK_SHARE_PROFILE_WARNING = 15,
  // CONFLICTING_MODULE = 16,  Deprecated
  CRITICAL_NOTIFICATION = 17,
  IME_WARNING = 18,
  TOOLBAR_ACTIONS_BAR = 19,
  GLOBAL_ERROR = 20,
  EXTENSION_INSTALL = 21,
  EXTENSION_UNINSTALL = 22,
  EXTENSION_INSTALLED = 23,
  PAYMENT_REQUEST = 24,
  SAVE_CARD = 25,
  CARD_UNMASK = 26,
  SIGN_IN = 27,
  SIGN_IN_SYNC_CONFIRMATION = 28,
  SIGN_IN_ERROR = 29,
  SIGN_IN_EMAIL_CONFIRMATION = 30,
  PROFILE_CHOOSER = 31,
  ACCOUNT_CHOOSER = 32,
  ARC_APP = 33,
  AUTO_SIGNIN_FIRST_RUN = 34,
  WEB_APP_CONFIRMATION = 35,
  CHOOSER_UI = 36,
  CHOOSER = 37,
  COLLECTED_COOKIES = 38,
  CONSTRAINED_WEB = 39,
  CONTENT_SETTING_CONTENTS = 40,
  CREATE_CHROME_APPLICATION_SHORTCUT = 41,
  DOWNLOAD_DANGER_PROMPT = 42,
  DOWNLOAD_IN_PROGRESS = 43,
  ECHO = 44,
  ENROLLMENT = 45,
  EXTENSION = 46,
  EXTENSION_POPUP_AURA = 47,
  EXTERNAL_PROTOCOL = 48,
  EXTERNAL_PROTOCOL_CHROMEOS = 49,
  FIRST_RUN_DIALOG = 50,
  HOME_PAGE_UNDO = 51,
  IDLE_ACTION_WARNING = 52,
  IMPORT_LOCK = 53,
  INTENT_PICKER = 54,
  INVERT = 55,
  JAVA_SCRIPT = 56,
  JAVA_SCRIPT_APP_MODAL_X11 = 57,
  LOGIN_HANDLER = 58,
  MANAGE_PASSWORDS = 59,
  MEDIA_GALLERIES = 60,
  MULTIPROFILES_INTRO = 61,
  MULTIPROFILES_SESSION_ABORTED = 62,
  NATIVE_CONTAINER = 63,
  NETWORK_CONFIG = 64,
  PERMISSIONS = 65,
  PLATFORM_KEYS_CERTIFICATE_SELECTOR = 66,
  PLATFORM_VERIFICATION = 67,
  PROXIMITY_AUTH_ERROR = 68,
  REQUEST_PIN = 69,
  SSL_CLIENT_CERTIFICATE_SELECTOR = 70,
  SIMPLE_MESSAGE_BOX = 71,
  TAB_MODAL_CONFIRM = 72,
  TASK_MANAGER = 73,
  TELEPORT_WARNING = 74,
  // USER_MANAGER = 75,  Deprecated
  // USER_MANAGER_PROFILE = 76,  Deprecated
  VALIDATION_MESSAGE = 77,
  WEB_SHARE_TARGET_PICKER = 78,
  ZOOM = 79,
  LOCK_SCREEN_NOTE_APP_TOAST = 80,
  PWA_CONFIRMATION = 81,
  RELAUNCH_RECOMMENDED = 82,
  CROSTINI_INSTALLER = 83,
  RELAUNCH_REQUIRED = 84,
  UNITY_SYNC_CONSENT_BUMP = 85,
  CROSTINI_UNINSTALLER = 86,
  DOWNLOAD_OPEN_CONFIRMATION = 87,
  ARC_DATA_REMOVAL_CONFIRMATION = 88,
  CROSTINI_UPGRADE = 89,
  HATS_BUBBLE = 90,
  CROSTINI_APP_RESTART = 91,
  INCOGNITO_WINDOW_COUNT = 92,
  CROSTINI_APP_UNINSTALLER = 93,
  CROSTINI_CONTAINER_UPGRADE = 94,
  COOKIE_CONTROLS = 95,
  CROSTINI_ANSIBLE_SOFTWARE_CONFIG = 96,
  INCOGNITO_MENU = 97,
  PHONE_CHOOSER = 98,
  QR_CODE_GENERATOR = 99,
  CROSTINI_FORCE_CLOSE = 100,
  APP_UNINSTALL = 101,
  PRINT_JOB_CONFIRMATION = 102,
  CROSTINI_RECOVERY = 103,
  PARENT_PERMISSION = 104,  // ChromeOS only.
  SIGNIN_REAUTH = 105,
  CURRENT_BROWSING_CONTEXT_CONFIRMATION_BOX = 106,
  PROFILE_PICKER_FORCE_SIGNIN = 107,
  EXTENSION_INSTALL_FRICTION = 108,
  // Add values above this line with a corresponding label in
  // tools/metrics/histograms/enums.xml
  MAX_VALUE
};

// Record an UMA metric counting the creation of a dialog box of this type.
void RecordDialogCreation(DialogIdentifier identifier);

#if defined(OS_WIN)

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

#endif  // OS_WIN

// Displays a dialog to notify the user that the extension installation is
// blocked due to policy. It also show additional information from administrator
// if it exists.
void ShowExtensionInstallBlockedDialog(
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
    std::unique_ptr<ChooserController> controller);
bool IsDeviceChooserShowingForTesting(Browser* browser);
#endif

// Show the prompt to set a window name for browser's window, optionally with
// the given context.
void ShowWindowNamePrompt(Browser* browser);
void ShowWindowNamePromptForTesting(Browser* browser,
                                    gfx::NativeWindow context);

}  // namespace chrome

void ShowFolderUploadConfirmationDialog(
    const base::FilePath& path,
    base::OnceCallback<void(const std::vector<ui::SelectedFileInfo>&)> callback,
    std::vector<ui::SelectedFileInfo> selected_files,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
