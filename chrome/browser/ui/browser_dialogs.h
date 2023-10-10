// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class LoginHandler;
class Profile;

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

namespace views {
class Widget;
}  // namespace views

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
views::Widget* ShowBrowserModal(Browser* browser,
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
    const absl::optional<std::u16string>& pin,
    content::BluetoothDelegate::PairPromptCallback close_callback);
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

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
