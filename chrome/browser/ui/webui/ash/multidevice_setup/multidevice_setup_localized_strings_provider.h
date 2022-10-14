// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_LOCALIZED_STRINGS_PROVIDER_H_

namespace login {
class LocalizedValuesBuilder;
}

namespace content {
class WebUIDataSource;
}

namespace ash::multidevice_setup {

// Adds the strings needed for the MultiDevice setup flow to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

// Same as AddLocalizedStrings but for a LocalizedValuesBuilder.
void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder);

}  // namespace ash::multidevice_setup

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos::multidevice_setup {
using ::ash::multidevice_setup::AddLocalizedValuesToBuilder;
}

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_SETUP_MULTIDEVICE_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
