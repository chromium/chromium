// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_

#include "base/values.h"

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::cellular_setup {

// Adds the strings needed for the cellular setup flow to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

// Same as AddLocalizedStrings() but for a LocalizedValuesBuilder.
void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder);

// Adds non-string constants for loadTimeData consumption.
void AddNonStringLoadTimeData(content::WebUIDataSource* html_source);

// Same as AddNonStringLoadTimeData() but for a Value::Dict.
void AddNonStringLoadTimeDataToDict(base::Value::Dict* dict);

}  // namespace ash::cellular_setup

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
