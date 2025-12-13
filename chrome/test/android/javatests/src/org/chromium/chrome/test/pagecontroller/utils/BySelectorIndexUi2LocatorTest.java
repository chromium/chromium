// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import androidx.test.uiautomator.By;
import androidx.test.uiautomator.BySelector;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link BySelectorIndexUi2Locator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public final class BySelectorIndexUi2LocatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private BySelector mSelector;

    @Mock private UiDevice mDevice;

    @Mock private UiObject2 mRoot;

    @Mock private UiObject2 mResult0;

    @Mock private UiObject2 mResult1;

    @Before
    public void setUp() {
        // Create a BySelectorLocator so tests can create BySelectorIndexUi2Locator off of it.
        mSelector = By.res("resource");
        List<UiObject2> results = new ArrayList<>();
        results.add(mResult0);
        results.add(mResult1);

        // Only mResult0 is used here since mSelector should return it when locateOne
        // is called.  This works because we've also correctly specified that it should
        // return mResults in response to locateAll, which will contain a list of mResult0
        // and mResult1.  BySelectorIndexUi2Locator uses locateAll to be able to locate
        // any child by it's position in the children list.
        TestUtils.stubMocks(mDevice, mRoot, mSelector, mResult0, results);
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
        new BySelectorIndexUi2Locator(mSelector, -1);
    }
}
