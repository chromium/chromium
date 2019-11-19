// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.android;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Android Permissions Dialog Page Controller.
 */
public class PermissionDialog extends PageController {
    private static final IUi2Locator LOCATOR_PERMISSION_DIALOG =
            Ui2Locators.withResName("com.android.packageinstaller:id/perm_desc_root");
    private static final IUi2Locator LOCATOR_ALLOW =
            Ui2Locators.withResName("com.android.packageinstaller:id/permission_allow_button");
    private static final IUi2Locator LOCATOR_DENY =
            Ui2Locators.withResName("com.android.packageinstaller:id/permission_deny_button");
    private static final IUi2Locator LOCATOR_MESSAGE =
            Ui2Locators.withResName("com.android.packageinstaller:id/permission_message");

    private static final PermissionDialog sInstance = new PermissionDialog();
    private PermissionDialog() {}
    public static PermissionDialog getInstance() {
        return sInstance;
    }

    public void clickAllow() {
        mUtils.click(LOCATOR_ALLOW);
    }

    public void clickDeny() {
        mUtils.click(LOCATOR_DENY);
    }

    public String getMessage() {
        return mLocatorHelper.getOneText(LOCATOR_MESSAGE);
    }

    @Override
    public PermissionDialog verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_PERMISSION_DIALOG);
        return this;
    }
}
