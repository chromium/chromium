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
 * Tests for PathUi2Locator
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class PathUi2LocatorTest {
    @Mock
    private IUi2Locator mLocator0;

    @Mock
    private IUi2Locator mLocator1;

    @Mock
    private IUi2Locator mLocator3;

    @Mock
    private IUi2Locator mLocator4;

    @Mock
    private UiObject2 mRoot;

    @Mock
    private UiObject2 mLocator0Result;

    @Mock
    private UiObject2 mLocator1Result;

    @Mock
    private UiObject2 mResult30;

    @Mock
    private UiObject2 mResult31;

    @Mock
    private UiObject2 mResult400;

    @Mock
    private UiObject2 mResult401;

    @Mock
    private UiObject2 mResult410;

    @Mock
    private UiObject2 mResult411;

    @Mock
    private UiDevice mDevice;

    private List<UiObject2> mLocator0Results;
    private List<UiObject2> mLocator1Results;
    private List<UiObject2> mResults3;
    private List<UiObject2> mResults40;
    private List<UiObject2> mResults41;
    private List<UiObject2> mResults4All;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mLocator0Results = Collections.singletonList(mLocator0Result);
        mLocator1Results = Collections.singletonList(mLocator1Result);

        mResults3 = new ArrayList<>();
        mResults3.add(mResult30);
        mResults3.add(mResult31);

        mResults40 = new ArrayList<>();
        mResults40.add(mResult400);
        mResults40.add(mResult401);

        mResults41 = new ArrayList<>();
        mResults41.add(mResult410);
        mResults41.add(mResult411);

        mResults4All = new ArrayList<>();
        mResults4All.addAll(mResults40);
        mResults4All.addAll(mResults41);

        when(mLocator0.locateAll(mRoot)).thenReturn(mLocator0Results);
        when(mLocator0.locateAll(mDevice)).thenReturn(mLocator0Results);
        when(mLocator1.locateAll(mLocator0Result)).thenReturn(mLocator1Results);
        when(mLocator3.locateAll(mDevice)).thenReturn(mResults3);
        when(mLocator3.locateAll(mRoot)).thenReturn(mResults3);
        when(mLocator4.locateAll(mResult30)).thenReturn(mResults40);
        when(mLocator4.locateAll(mResult31)).thenReturn(mResults41);
    }

    @Test
    public void locateTrivialPath() {
        PathUi2Locator locator = new PathUi2Locator(mLocator0);
        assertLocatorResults(mDevice, mRoot, locator, mLocator0Result, mLocator0Results);
    }

    @Test
    public void locateNonForkingPathDevice() {
        PathUi2Locator locator = new PathUi2Locator(mLocator0, mLocator1);
        assertLocatorResults(mDevice, mRoot, locator, mLocator1Result, mLocator1Results);
    }

    @Test
    public void locateForkingPath() {
        PathUi2Locator locator = new PathUi2Locator(mLocator3, mLocator4);
        assertLocatorResults(mDevice, mRoot, locator, mResult400, mResults4All);
    }
}
