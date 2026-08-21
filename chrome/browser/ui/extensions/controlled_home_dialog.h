// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_H_

#include <memory>

#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"

class ControlledHomeDialogControllerInterface;
class Profile;

namespace extensions {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kControlledHomeDialogCancelButtonElementId);

// Shows a dialog to notify the user when an extension has changed the home
// page.
void ShowControlledHomeDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    std::unique_ptr<ControlledHomeDialogControllerInterface> controller);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CONTROLLED_HOME_DIALOG_H_
