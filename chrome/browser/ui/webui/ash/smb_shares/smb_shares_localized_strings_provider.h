// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARES_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARES_LOCALIZED_STRINGS_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace ash::smb_dialog {

// Adds the strings needed for SMB shares to |html_source|.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace ash::smb_dialog

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SMB_SHARES_SMB_SHARES_LOCALIZED_STRINGS_PROVIDER_H_
