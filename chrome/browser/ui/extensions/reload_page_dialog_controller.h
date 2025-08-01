// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_RELOAD_PAGE_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_RELOAD_PAGE_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kReloadPageDialogCancelButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kReloadPageDialogOkButtonElementId);

// A controller for a dialog that prompts the user to reload the active page so
// that a given list of extensions can run.
class ReloadPageDialogController {
 public:
  // Information for an extension that should be included in the dialog.
  struct ExtensionInfo {
    ExtensionId id;
    std::string name;
    gfx::Image icon;
  };

  ReloadPageDialogController(gfx::NativeWindow parent,
                             content::BrowserContext* browser_context,
                             base::OnceClosure callback);
  ~ReloadPageDialogController();
  ReloadPageDialogController(const ReloadPageDialogController&) = delete;
  const ReloadPageDialogController& operator=(
      const ReloadPageDialogController&) = delete;

  // Starts the process of showing the dialog for the given `extensions`.
  void TriggerShow(const std::vector<const Extension*>& extensions);

 private:
  // Shows the reload page dialog with the extensions information gathered in
  // `extensions_info_`.
  void Show();

  // Called when an extension's icon has finished loading. `done_callback` is
  // used to track when all extensions icons have been loaded.
  void OnExtensionIconLoaded(const ExtensionId& extension_id,
                             const std::string& extension_name,
                             base::OnceClosure done_callback,
                             const gfx::Image& icon);

  gfx::NativeWindow parent_;
  raw_ptr<content::BrowserContext> browser_context_;

  // Information for the extensions to be displayed in the dialog.
  std::vector<ExtensionInfo> extensions_info_;

  // The callback to be run when the user accepts the dialog.
  // TODO(crbug.com/424012380): move callback from extension action runner.
  base::OnceClosure on_dialog_accepted_;

  base::WeakPtrFactory<ReloadPageDialogController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_RELOAD_PAGE_DIALOG_CONTROLLER_H_
