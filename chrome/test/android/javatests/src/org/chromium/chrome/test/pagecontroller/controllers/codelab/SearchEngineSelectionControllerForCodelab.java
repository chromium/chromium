// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.controllers.codelab;

import org.chromium.chrome.test.pagecontroller.controllers.PageController;

// TODO: Implement page controller for SearchEnginePreferences.java.

/**
 * Search Engine Selection Page Controller for the Code Lab, representing
 * SearchEnginePreferences.java.
 *
 * @see <a
 *         href="https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/android/java/src/org/chromium/chrome/browser/preferences/SearchEnginePreference.java">SearchEnginePreference.java</a>
 */

public class SearchEngineSelectionControllerForCodelab extends PageController {
    // The next 5 lines are boilerplate, no need to modify.
    private static final SearchEngineSelectionControllerForCodelab sInstance =
            new SearchEngineSelectionControllerForCodelab();
    private SearchEngineSelectionControllerForCodelab() {}
    public static SearchEngineSelectionControllerForCodelab getInstance() {
        return sInstance;
    }

    @Override
    public SearchEngineSelectionControllerForCodelab verifyActive() {
        // TODO: Implement this method to verify that the UI is displaying the
        // search engine selection activity then return this (otherwise throw).

        return this;
    }

    /**
     * Choose the Omnibox default search engine.
     * @param  engine The engine to choose.
     * @return        SearchEngineSelectionControllerForCodelab, after verification that
     *                the page has transitioned to it.
     */
    public SearchEngineSelectionControllerForCodelab chooseSearchEngine(String engineName) {
        // TODO: Construct a IUi2Locator for the element corresponding to the
        // given engineName and perform a click on it.
        // (Hint, the resource id entry for search engine choices along with the
        // engineName can be used by Ui2Locators.withPath(...).)

        return this;
    }

    /**
     * @return The current engine choice.
     */
    public String getEngineChoice() {
        // TODO: Determine which engine option is selected and return it.
        // (Hint, there are various get*Checked methods in UILocatorHelper
        // to find out if something is checked.)

        return null;
    }
}
