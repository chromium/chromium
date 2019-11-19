// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.pagecontroller.utils.TestUtils.assertLocatorResults;

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
 * Tests for IndexUi2Locator
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class IndexUi2LocatorTest {
    @Mock
    private IUi2Locator mLocator0;

    @Mock
    private UiObject2 mResult0;

    @Mock
    private UiObject2 mResult1;

    @Mock
    private List<UiObject2> mLocatorResults0;

    @Mock
    private List<UiObject2> mLocatorResults1;

    @Mock
    private UiObject2 mRoot;

    @Mock
    private UiDevice mDevice;

    private List<UiObject2> mLocatorResults;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mLocatorResults0 = Collections.singletonList(mResult0);
        mLocatorResults1 = Collections.singletonList(mResult1);

        mLocatorResults = new ArrayList<>();
        mLocatorResults.add(mResult0);
        mLocatorResults.add(mResult1);

        when(mLocator0.locateAll(mRoot)).thenReturn(mLocatorResults);
        when(mLocator0.locateAll(mDevice)).thenReturn(mLocatorResults);
    }

    @Test
    public void locateFirst() {
        IndexUi2Locator locator = new IndexUi2Locator(0, mLocator0);
        assertLocatorResults(mDevice, mRoot, locator, mResult0, mLocatorResults0);
    }

    @Test
    public void locateSecond() {
        IndexUi2Locator locator = new IndexUi2Locator(1, mLocator0);
        assertLocatorResults(mDevice, mRoot, locator, mResult1, mLocatorResults1);
    }
}
