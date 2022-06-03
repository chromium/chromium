// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_

namespace base {
class DictionaryValue;
}  // namespace base

namespace login {
class LocalizedValuesBuilder;
}  // namespace login

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace cellular_setup {

// Adds the strings needed for the cellular setup flow to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

// Same as AddLocalizedStrings() but for a LocalizedValuesBuilder.
void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder);

// Adds non-string constants for loadTimeData consumption.
void AddNonStringLoadTimeData(content::WebUIDataSource* html_source);

// Same as AddNonStringLoadTimeData() but for a DictionaryValue.
void AddNonStringLoadTimeDataToDict(base::DictionaryValue* dict);

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CELLULAR_SETUP_CELLULAR_SETUP_LOCALIZED_STRINGS_PROVIDER_H_
