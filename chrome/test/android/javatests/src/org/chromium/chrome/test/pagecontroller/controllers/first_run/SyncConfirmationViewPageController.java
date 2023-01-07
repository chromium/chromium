// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.first_run;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Sync Dialog (part of First Run Experience) Page Controller.
 */
public class SyncConfirmationViewPageController extends PageController {
    private static final IUi2Locator LOCATOR_SYNC_CONFIRMATION_VIEW =
            Ui2Locators.withAnyResEntry(R.id.signin_sync_title);
    private static final IUi2Locator LOCATOR_NO_THANKS =
            Ui2Locators.withAnyResEntry(R.id.negative_button);

    private static final SyncConfirmationViewPageController sInstance =
            new SyncConfirmationViewPageController();
    private SyncConfirmationViewPageController() {}
    public static SyncConfirmationViewPageController getInstance() {
        return sInstance;
    }

    public void clickNoThanks() {
        mUtils.click(LOCATOR_NO_THANKS);
    }

    @Override
    public SyncConfirmationViewPageController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_SYNC_CONFIRMATION_VIEW);
        return this;
    }
}
