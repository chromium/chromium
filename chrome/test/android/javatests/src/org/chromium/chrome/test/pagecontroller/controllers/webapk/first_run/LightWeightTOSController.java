// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.webapk.first_run;

import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;

/**
 * Page Controller for the "light weight" version of the TOS dialog. Shown instead of the
 * normal TOS dialog when light weight first run experience is preferred, for example, when
 * a WebAPK is launched..
 */
public class LightWeightTOSController extends PageController {
    private static final IUi2Locator LOCATOR_TOS =
            Ui2Locators.withAnyResEntry(R.id.lightweight_fre_tos_and_privacy);
    private static final IUi2Locator LOCATOR_DUAL_CONTROL =
            Ui2Locators.withAnyResEntry(R.id.lightweight_fre_buttons);
    private static final IUi2Locator LOCATOR_ACCEPT_BUTTON =
            Ui2Locators.withAnyResEntry(R.id.button_primary);

    private static final LightWeightTOSController sInstance = new LightWeightTOSController();
    private LightWeightTOSController() {}
    public static LightWeightTOSController getInstance() {
        return sInstance;
    }

    @Override
    public LightWeightTOSController verifyActive() {
        mLocatorHelper.verifyOnScreen(LOCATOR_TOS);
        mLocatorHelper.verifyOnScreen(LOCATOR_DUAL_CONTROL);
        mLocatorHelper.verifyOnScreen(LOCATOR_ACCEPT_BUTTON);
        return this;
    }

    public void acceptAndContinue() {
        mUtils.click(LOCATOR_ACCEPT_BUTTON);
    }
}
