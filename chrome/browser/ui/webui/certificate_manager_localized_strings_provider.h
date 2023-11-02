// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_LOCALIZED_STRINGS_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace certificate_manager {

// Adds the strings needed for the certificate_manager component to
// |html_source|. String ids correspond to ids in
// ui/webui/resources/cr_components/certificate_manager/.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace certificate_manager

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_LOCALIZED_STRINGS_PROVIDER_H_
