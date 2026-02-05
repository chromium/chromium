// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_DIALOGS_SETTINGS_OVERRIDDEN_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_DIALOGS_SETTINGS_OVERRIDDEN_DIALOG_H_

#include "extensions/buildflags/buildflags.h"
#include "ui/base/interaction/element_identifier.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

inline constexpr char kExtensionSettingsOverriddenDialogName[] =
    "ExtensionSettingsOverriddenDialog";

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kSettingsOverriddenDialogPreviousSettingButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogNewSettingButtonId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSettingsOverriddenDialogSaveButtonId);

#endif  // CHROME_BROWSER_UI_EXTENSIONS_DIALOGS_SETTINGS_OVERRIDDEN_DIALOG_H_
