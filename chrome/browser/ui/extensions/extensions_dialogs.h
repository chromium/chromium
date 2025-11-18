// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "extensions/common/extension_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/files/safe_base_name.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class Browser;
class ControlledHomeDialogControllerInterface;
class SettingsOverriddenDialogController;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace permissions {
class ChooserController;
}  // namespace permissions

namespace extensions {

class Extension;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kControlledHomeDialogCancelButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kExtensionInstallFrictionLearnMoreLink);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMv2DisabledDialogManageButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMv2DisabledDialogParagraphElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMv2DisabledDialogRemoveButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMv2KeepDialogOkButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kParentBlockedDialogMessage);

void ShowConstrainedDeviceChooserDialog(
    content::WebContents* web_contents,
    std::unique_ptr<permissions::ChooserController> controller);

// Shows a dialog to notify the user that the extension installation is
// blocked due to policy. It also shows additional information from
// administrator if it exists.
void ShowExtensionInstallBlockedDialog(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const std::u16string& custom_error_message,
    const gfx::ImageSkia& icon,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);

// Shows a dialog to notify the user when an extension has changed the home
// page.
void ShowControlledHomeDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    std::unique_ptr<ControlledHomeDialogControllerInterface> controller);

// Shows a dialog that prompts the user for whether to open a DownloadItem using
// native UI. This step is necessary to prevent a malicious extension from
// opening any downloaded file.
void ShowDownloadOpenConfirmationDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> open_callback);

// Shows a modal dialog to Enhanced Safe Browsing users before the extension
// install dialog if the extension is not included in the Safe Browsing CRX
// allowlist. `callback` will be invoked with `true` if the user accepts or
// `false` if the user cancels the dialog.
void ShowExtensionInstallFrictionDialog(
    content::WebContents* contents,
    base::OnceCallback<void(bool)> callback);

// Shows a model dialog to users when they uninstall multiple extensions.
// When the dialog is accepted, `accept_callback` is invoked.
// When the dialog is canceled, `cancel_callback` is invoked.
void ShowExtensionMultipleUninstallDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    const std::vector<ExtensionId>& extension_ids,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback);

// Shows a dialog with `extensions_info` when those extensions were disabled due
// to the MV2 deprecation.
void ShowMv2DeprecationDisabledDialog(
    Browser* browser,
    std::vector<Mv2DisabledDialogController::ExtensionInfo>& extensions_info,
    base::OnceClosure remove_callback,
    base::OnceClosure manage_callback,
    base::OnceClosure close_callback);

// Shows a dialog when the user triggers the warning dismissal for an extension
// affected by the MV2 deprecation.
void ShowMv2DeprecationKeepDialog(Browser* browser,
                                  const Extension& extension,
                                  base::OnceClosure accept_callback,
                                  base::OnceClosure cancel_callback);

// Shows a dialog when the user re-enables an extension affected by the MV2
// deprecation.
void ShowMv2DeprecationReEnableDialog(
    gfx::NativeWindow parent,
    const ExtensionId& extension_id,
    const std::string& extension_name,
    base::OnceCallback<void(bool)> done_callback);

// Shows a dialog with a warning to the user that their settings have been
// overridden by an extension.
void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    gfx::NativeWindow parent);

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
    const Extension* extension,
    content::WebContents* web_contents,
    base::OnceClosure done_callback);

// Shows a dialog when the user tries to upload an extension to their account.
void ShowUploadExtensionToAccountDialog(Browser* browser,
                                        const Extension& extension,
                                        base::OnceClosure accept_callback,
                                        base::OnceClosure cancel_callback);

#if BUILDFLAG(IS_CHROMEOS)

// Shows a scanner discovery confirmation dialog bubble anchored to the toolbar
// icon for the extension.  If there's no toolbar icon or parent, it will
// display a browser-modal dialog instead.
void ShowDocumentScannerDiscoveryConfirmationDialog(
    gfx::NativeWindow parent,
    const ExtensionId& extension_id,
    const std::u16string& extension_name,
    const gfx::ImageSkia& extension_icon,
    base::OnceCallback<void(bool)> callback);

// Shows a start scan confirmation dialog bubble anchored to the toolbar icon
// for the extension.  If there's no toolbar icon or parent, it will display a
// browser-modal dialog instead.
void ShowDocumentScannerStartScanConfirmationDialog(
    gfx::NativeWindow parent,
    const ExtensionId& extension_id,
    const std::u16string& extension_name,
    const std::u16string& scanner_name,
    const gfx::ImageSkia& extension_icon,
    base::OnceCallback<void(bool)> callback);

// Shows a dialog requesting the user to grant the extension access to a file
// system.
void ShowRequestFileSystemDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::mojom::DialogButton)> callback);

// Shows the print job confirmation dialog bubble anchored to the toolbar icon
// for the extension.  If there's no toolbar icon or parent, it will display a
// browser-modal dialog instead.
void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const ExtensionId& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback);

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
