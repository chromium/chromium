// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.codelab;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;

// TODO: Implement page controller for MainPreferences.java.  Read
// documentation in the PageController class and refer to implemented Page
// Controllers in the pagecontroller directory for examples.

/**
 * Settings Menu Page Controller for the Code Lab, representing
 * MainPreferences.java.
 *
 * @see <a
 *         href="https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/android/java/src/org/chromium/chrome/browser/preferences/MainPreferences.java">MainPreferences.java</a>
 */
public class SettingsControllerForCodelab extends PageController {
    // TODO: Replace null with a an actual locator.  Hint, see
    // {@link ../../README.md#where-to-find-the-resource-and-string-ids}, and
    // {@link ../../utils/Ui2Locators#withTextString}.
    static final IUi2Locator LOCATOR_SETTINGS = null;

    // TODO: (Hint, you may need more IUi2Locators, add them here.

    // The next 5 lines are boilerplate, no need to modify.
    private static final SettingsControllerForCodelab sInstance =
            new SettingsControllerForCodelab();
    private SettingsControllerForCodelab() {}
    public static SettingsControllerForCodelab getInstance() {
        return sInstance;
    }

    @Override
    public SettingsControllerForCodelab verifyActive() {
        // TODO: See PageController.verifyActive documentation on what this
        // method should do.  (Hint, PageController has a mLocatorHelper field.)

        return this;
    }

    /**
     * Click on the Search Engine option.
     * @returns SearchEngineSelectionControllerForCodelab PageController.
     */
    public SearchEngineSelectionControllerForCodelab clickSearchEngine() {
        // TODO: Perform a click on the Search engine option
        // in the Settings menu.  (Hint, PageController has a mUtils field.)

        // TODO: Replace null with an instance of CodelabSearchEngine.  (Hint,
        // all PageController subclasses have a verifyActive method.)
        return null;
    }
}
