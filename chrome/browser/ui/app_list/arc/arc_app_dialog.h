// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_DIALOG_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_DIALOG_H_

#include <string>

#include "base/callback.h"

class AppListControllerDelegate;
class Profile;

namespace arc {

using ArcUsbConfirmCallback = base::OnceCallback<void(bool)>;

// Shows a dialog for user to confirm uninstallation of ARC app.
// Currently, ARC app can only be manually uninstalled from AppList. But it
// would be simple to enable the dialog to shown from other source.
void ShowArcAppUninstallDialog(Profile* profile,
                               AppListControllerDelegate* controller,
                               const std::string& app_id);

// Shows permission request dialog for scan USB device list.
void ShowUsbScanDeviceListPermissionDialog(Profile* profile,
                                           const std::string& app_id,
                                           ArcUsbConfirmCallback callback);

// Shows permission request dialog for targeting device name.
void ShowUsbAccessPermissionDialog(Profile* profile,
                                   const std::string& app_id,
                                   const base::string16& device_name,
                                   ArcUsbConfirmCallback callback);

// Test purpose methods.
bool IsArcAppDialogViewAliveForTest();

bool CloseAppDialogViewAndConfirmForTest(bool confirm);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_DIALOG_H_
