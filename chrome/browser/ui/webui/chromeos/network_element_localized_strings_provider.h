// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_

namespace login {
class LocalizedValuesBuilder;
}

namespace content {
class WebUIDataSource;
}

namespace chromeos {
namespace network_element {

// Adds the strings needed for network elements to |html_source|. String ids
// correspond to ids in ui/webui/resources/cr_components/chromeos/network/.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

// Same as AddLocalizedStrings but for a LocalizedValuesBuilder.
void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder);

// Adds ONC strings used by the details dialog used in Settings and WebUI.
void AddOncLocalizedStrings(content::WebUIDataSource* html_source);

// Adds strings used by the details dialog used in Settings and WebUI.
void AddDetailsLocalizedStrings(content::WebUIDataSource* html_source);

// Adds strings used by the configuration dialog used in Settings and
// WebUI.
void AddConfigLocalizedStrings(content::WebUIDataSource* html_source);

// Adds error strings for networking UI.
void AddErrorLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace network_element
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_NETWORK_ELEMENT_LOCALIZED_STRINGS_PROVIDER_H_
