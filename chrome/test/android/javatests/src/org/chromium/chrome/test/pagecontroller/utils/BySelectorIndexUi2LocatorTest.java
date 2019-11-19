// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject2;

import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests for BySelectorIndexUi2Locator.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public final class BySelectorIndexUi2LocatorTest {
    private BySelector mSelector;

    @Mock
    private UiDevice mDevice;

    @Mock
    private UiObject2 mRoot;

    @Mock
    private UiObject2 mResult0;

    @Mock
    private UiObject2 mResult1;

    private List<UiObject2> mResults;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Create a BySelectorLocator so tests can create BySelectorIndexUi2Locator off of it.
        mSelector = By.res("resource");
        mResults = new ArrayList<>();
        mResults.add(mResult0);
        mResults.add(mResult1);

        // Only mResult0 is used here since mSelector should return it when locateOne
        // is called.  This works because we've also correctly speicifed that it should
        // return mResults in response to locateAll, which will contain a list of mResult0
        // and mResult1.  BySelectorIndexUi2Locator uses locateAll to be able to locate
        // any child by it's position in the children list.
        TestUtils.stubMocks(mDevice, mRoot, mSelector, mResult0, mResults);
    }

    @Test
    public void locateFirst() {
        BySelectorIndexUi2Locator locator = new BySelectorIndexUi2Locator(mSelector, 0);
        TestUtils.assertLocatorResults(
                mDevice, mRoot, locator, mResult0, Collections.singletonList(mResult0));
    }

    @Test
    public void locateSecond() {
        BySelectorIndexUi2Locator locator = new BySelectorIndexUi2Locator(mSelector, 1);
        TestUtils.assertLocatorResults(
                mDevice, mRoot, locator, mResult1, Collections.singletonList(mResult1));
    }

    @Test
    public void locateOutOfBounds() {
        BySelectorIndexUi2Locator locator = new BySelectorIndexUi2Locator(mSelector, 2);
        TestUtils.assertLocatorResults(mDevice, mRoot, locator, null, Collections.emptyList());
    }

    @Test(expected = IllegalArgumentException.class)
    public void locateNegativeIndex() {
        BySelectorIndexUi2Locator locator = new BySelectorIndexUi2Locator(mSelector, -1);
    }
}
