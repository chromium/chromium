// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.notifications;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Download Permissions Dialog (download for offline viewing) Page Controller.
 */
public class DownloadNotificationController extends PageController {
    private static final IUi2Locator LOCATOR_DOWNLOAD_NOTIFICATION =
            Ui2Locators.withTextContaining("needs storage access to download files");
    private static final IUi2Locator LOCATOR_CONTINUE =
            Ui2Locators.withAnyResEntry(android.R.id.button1);

    private static final DownloadNotificationController sInstance =
            new DownloadNotificationController();
    private DownloadNotificationController() {}
    public static DownloadNotificationController getInstance() {
        return sInstance;
    }

    public void clickContinue() {
        mUtils.click(LOCATOR_CONTINUE);
    }

    @Override
    public DownloadNotificationController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_DOWNLOAD_NOTIFICATION);
        return this;
    }
}
