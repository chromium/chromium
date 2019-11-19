// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers;

import android.os.RemoteException;

import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;

/**
 * Base class for Page Controllers.
 *
 * A Page Controller allows tests to interact with a single modal view in the
 * app-under-test.  "Page" here does not mean a web-page, but more like Android
 * Activity.  Although special pages such as "chrome://version" could be
 * implemented as a distinct PageController which other normal webpages can be
 * loaded in a more generic UrlPage controller.  Modal menus or dialogs
 * controllers are also appropriate to implement as PageControllers.
 *
 * When implementing methods that would otherwise return void, but does not
 * navigate to another PageController, consider returning "this" to facilitate
 * chaining.
 *
 * When implementing methods that navigates to another PageController, consider
 * returning a verified instance of that controller by returning the result of
 * its verifyActive() method to facilitate chaining.  An exception to this
 * would be if experiments could cause navigation to different PageControllers,
 * then the return value should be void.
 */
public abstract class PageController extends ElementController {
    public void pressAndroidBackButton() {
        mUtils.pressBack();
    }

    public void pressAndroidHomeButton() {
        mUtils.pressHome();
    }

    public void pressAndroidOverviewButton() {
        try {
            // UiDevice (used by UiAutomatorUtils) calls this the recent apps button,
            // Android UI seems to prefer the overview button name (as evidenced by talkback's
            // readout)
            mUtils.pressRecentApps();
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Checks if the current page displayed corresponds to this page controller.
     * @return True if current page can be controlled by this controller, else false.
     */
    public final boolean isCurrentPageThis() {
        try {
            verifyActive();
            return true;
        } catch (UiLocationException e) {
            return false;
        }
    }

    /**
     * Verifies that the current page belongs to the controller.
     * @throws           UiLocationException if the current page does not belong the controller.
     */
    public abstract PageController verifyActive();
}
