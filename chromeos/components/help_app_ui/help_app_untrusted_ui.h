// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UNTRUSTED_UI_H_
#define CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UNTRUSTED_UI_H_

namespace content {
class WebUIDataSource;
}

class HelpAppUIDelegate;

namespace chromeos {
// The data source creation for chrome-untrusted://help-app.
content::WebUIDataSource* CreateHelpAppUntrustedDataSource(
    HelpAppUIDelegate* delegate);

// The data source creation for chrome-untrusted://help-app-kids-magazine.
content::WebUIDataSource* CreateHelpAppKidsMagazineUntrustedDataSource();
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_UNTRUSTED_UI_H_
