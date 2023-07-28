// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

class Browser;
class SettingsOverriddenDialogController;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {

class Extension;

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

// Shows a dialog when extensions require a refresh for their action
// to be run or blocked. When the dialog is accepted, `callback` is
// invoked.
void ShowReloadPageDialog(
    Browser* browser,
    const std::vector<extensions::ExtensionId>& extension_ids,
    base::OnceClosure callback);

// Shows a dialog with a warning to the user that their settings have been
// overridden by an extension.
void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    Browser* browser);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)

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

#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS)

// Shows a dialog requesting the user to grant the extension access to a file
// system.
void ShowRequestFileSystemDialog(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const std::string& volume_label,
    bool writable,
    base::OnceCallback<void(ui::DialogButton)> callback);

// Shows the print job confirmation dialog bubble anchored to the toolbar icon
// for the extension.
// If there's no toolbar icon, shows a modal dialog using
// CreateBrowserModalDialogViews(). Note that this dialog is shown even if there
// is no `parent` window.
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
