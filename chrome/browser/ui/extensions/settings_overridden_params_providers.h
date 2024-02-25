// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_

#include <optional>

#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

namespace settings_overridden_params {

// Retrieves the params for displaying the NTP setting overridden dialog, if
// there is a controlling extension. Otherwise, returns an empty optional.
std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile);

// Retrieves the params for displaying the dialog indicating that the default
// search engine has been overridden, if there is a controlling extension.
// Otherwise, returns an empty optional.
std::optional<ExtensionSettingsOverriddenDialog::Params>
GetSearchOverriddenParams(Profile* profile);

}  // namespace settings_overridden_params

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
