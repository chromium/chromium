// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_GUEST_UI_H_
#define CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_GUEST_UI_H_

namespace content {
class WebUIDataSource;
}

namespace chromeos {
// The data source creation for chrome://help-app-guest.
content::WebUIDataSource* CreateHelpAppGuestDataSource();
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_HELP_APP_UI_HELP_APP_GUEST_UI_H_
