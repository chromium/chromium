// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_ui_types.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class ControlledHomeDialogControllerInterface;
class SettingsOverriddenDialogController;
class Profile;

namespace content {
class WebContents;
}

namespace custom_handlers {
class ProtocolHandler;
}  // namespace custom_handlers

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace permissions {
class ChooserController;
}  // namespace permissions

namespace url {
class Origin;
}  // namespace url

namespace extensions {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kControlledHomeDialogCancelButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kMv2KeepDialogOkButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogHandlerRedirection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogRememberMeCheckbox);

void ShowConstrainedDeviceChooserDialog(
    content::WebContents* web_contents,
    std::unique_ptr<permissions::ChooserController> controller);

// Shows a dialog to notify the user when an extension has changed the home
// page.
void ShowControlledHomeDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    std::unique_ptr<ControlledHomeDialogControllerInterface> controller);

// Shows a dialog with a warning to the user that their settings have been
// overridden by an extension.
void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    gfx::NativeWindow parent);

#if !BUILDFLAG(IS_ANDROID)
// Shows a dialog when the user tries to perform a navigation and the target url
// has a protocol handler registered by an extension to handle the url's scheme.
void ShowConfirmProtocolHandlerDialog(
    content::WebContents* web_contents,
    const custom_handlers::ProtocolHandler& handler,
    const std::optional<url::Origin>& initiating_origin,
    base::OnceCallback<void(bool)> granted_callback,
    base::OnceCallback<void()> denied_callback);
#endif  // !BUILDFLAG(IS_ANDROID)

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
