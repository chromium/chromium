// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_H_

#include "extensions/buildflags/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

inline constexpr char kExtensionSettingsOverriddenDialogName[] =
    "ExtensionSettingsOverriddenDialog";

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kSettingsOverriddenDialogPreviousSettingButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogNewSettingButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogSaveButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogKeepItButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogParagraphId);

class SettingsOverriddenDialogController;

namespace extensions {

// Shows a dialog with a warning to the user that their settings have been
// overridden by an extension.
void ShowSettingsOverriddenDialog(
    std::unique_ptr<SettingsOverriddenDialogController> controller,
    gfx::NativeWindow parent);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_H_
