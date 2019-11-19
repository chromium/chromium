// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.first_run;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Data Saver Dialog (part of the First Run Experience) Page Controller.
 */
public class DataSaverController extends PageController {
    private static final IUi2Locator LOCATOR_DATA_SAVER =
            Ui2Locators.withAnyResEntry(R.id.enable_data_saver_switch);
    private static final IUi2Locator LOCATOR_NEXT = Ui2Locators.withAnyResEntry(R.id.next_button);

    private static final DataSaverController sInstance = new DataSaverController();
    private DataSaverController() {}
    public static DataSaverController getInstance() {
        return sInstance;
    }

    public void clickNext() {
        mUtils.click(LOCATOR_NEXT);
    }

    @Override
    public DataSaverController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_DATA_SAVER);
        return this;
    }
}
