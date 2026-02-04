// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_UTILS_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "ui/base/models/dialog_model.h"

namespace extensions {

// Adds a pair of custom radio buttons to the dialog model, offering a choice
// of which setting to use. The buttons are returned inside a container,
// allowing the radio buttons to be grouped, as well as to support any required
// customization.
void AddExplicitChoiceRadioButtons(
    ui::DialogModel::Builder& builder,
    const SettingsOverriddenDialogController::SettingOption& option1,
    ui::ElementIdentifier id1,
    base::RepeatingClosure callback1,
    const SettingsOverriddenDialogController::SettingOption& option2,
    ui::ElementIdentifier id2,
    base::RepeatingClosure callback2);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SETTINGS_OVERRIDDEN_DIALOG_VIEW_UTILS_H_
