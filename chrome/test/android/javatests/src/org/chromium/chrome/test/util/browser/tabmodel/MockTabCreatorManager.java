// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** MockTabCreatorManager for use in tests. */
public class MockTabCreatorManager implements TabCreatorManager {
    private MockTabCreator mRegularCreator;
    private MockTabCreator mIncognitoCreator;

    public MockTabCreatorManager() {}

    public MockTabCreatorManager(TabModelSelector selector) {
        initialize(selector);
    }

    public void initialize(TabModelSelector selector) {
        mRegularCreator = new MockTabCreator(false, selector);
        mIncognitoCreator = new MockTabCreator(true, selector);
    }

    @Override
    public MockTabCreator getTabCreator(boolean incognito) {
        return incognito ? mIncognitoCreator : mRegularCreator;
    }
}