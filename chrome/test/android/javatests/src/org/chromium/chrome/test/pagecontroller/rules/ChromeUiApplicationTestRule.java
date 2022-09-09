// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.rules;

import android.support.test.InstrumentationRegistry;

import org.junit.rules.ExternalResource;

import org.chromium.base.Log;
import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.SyncConfirmationViewPageController;
import org.chromium.chrome.test.pagecontroller.controllers.first_run.TOSController;
import org.chromium.chrome.test.pagecontroller.controllers.ntp.NewTabPageController;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocationException;

/**
 * Test rule that provides access to a Chrome application.
 */
public class ChromeUiApplicationTestRule extends ExternalResource {
    public static final String PACKAGE_NAME_ARG = "org.chromium.chrome.test.pagecontroller.rules."
            + "ChromeUiApplicationTestRule.PackageUnderTest";
    private static final String TAG = "ChromeUiAppTR";

    private String mPackageName;

    /**
     * Returns an instance of the page controller that corresponds to the current page.
     * @param controllers      List of possible page controller instances to search among.
     * @return                 The detected page controller.
     * @throws UiLocationException If page can't be determined.
     */
    public static PageController detectPageAmong(PageController... controllers) {
        for (PageController instance : controllers) {
            if (instance.isCurrentPageThis()) {
                Log.d(TAG, "Detected " + instance.getClass().getName());
                return instance;
            }
        }
        throw new UiLocationException("Could not detect current Page.");
    }

    /** Launch the chrome application */
    public void launchApplication() {
        UiAutomatorUtils utils = UiAutomatorUtils.getInstance();
        utils.launchApplication(mPackageName);
    }

    /**
     *  Navigate to the New Tab Page after the application starts for the first time, or after
     *  application data was cleared.
     *  @return NewTabPageController
     */
    public NewTabPageController navigateToNewTabPageOnFirstRun() {
        PageController controller = detectPageOnFirstRun();
        if (controller instanceof TOSController) {
            ((TOSController) controller).acceptAndContinue();
            controller = detectPageOnFirstRun();
        }
        if (controller instanceof SyncConfirmationViewPageController) {
            ((SyncConfirmationViewPageController) controller).clickNoThanks();
            controller = detectPageOnFirstRun();
        }
        if (controller instanceof NewTabPageController) {
            return (NewTabPageController) controller;
        } else {
            throw new UiLocationException("Could not navigate to new tab page from "
                    + controller.getClass().getName() + ".");
        }
    }

    /** Launch the application and navigate to the New Tab Page */
    public NewTabPageController launchIntoNewTabPageOnFirstRun() {
        launchApplication();
        return navigateToNewTabPageOnFirstRun();
    }

    public String getApplicationPackage() {
        return mPackageName;
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        mPackageName = InstrumentationRegistry.getArguments().getString(PACKAGE_NAME_ARG);
        // If the runner didn't pass the package name under test to us, then we can assume
        // that the target package name provided in the AndroidManifest is the app-under-test.
        if (mPackageName == null) {
            mPackageName = InstrumentationRegistry.getTargetContext().getPackageName();
        }
    }

    /**
     * Detect the page controller from among page controllers that could be displayed on first
     * launch or after application data was cleared.
     * Add potential page controllers that could show up before the New Tab Page here.
     * @return                 The detected page controller.
     * @throws UiLocationException If page can't be determined.
     */
    private static PageController detectPageOnFirstRun() {
        return detectPageAmong(TOSController.getInstance(),
                SyncConfirmationViewPageController.getInstance(),
                NewTabPageController.getInstance());
    }
}
