// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import static org.junit.Assert.assertEquals;
import static org.mockito.Matchers.argThat;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.pagecontroller.utils.TestUtils.matchesByDepth;

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
 * Tests for ChildIndexUi2Locator
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class ChildIndexUi2LocatorTest {
    @Mock
    private UiDevice mDevice;

    @Mock
    private UiObject2 mNode0;

    @Mock
    private UiObject2 mNode1;

    @Mock
    private UiObject2 mNode00;

    @Mock
    private UiObject2 mNode01;

    @Mock
    private UiObject2 mNode10;

    @Mock
    private UiObject2 mNode11;

    @Mock
    private UiObject2 mNode110;

    @Mock
    private UiObject2 mNode111;

    private List<UiObject2> mNodeList;
    private List<UiObject2> mNode0Children;
    private List<UiObject2> mNode1Children;
    private List<UiObject2> mNode11Children;

    private ChildIndexUi2Locator mChildLocator;

    private ChildIndexUi2Locator mGrandChildLocator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mNodeList = new ArrayList<>();
        mNodeList.add(mNode0);
        mNodeList.add(mNode1);
        mNode0Children = new ArrayList<>();
        mNode0Children.add(mNode00);
        mNode0Children.add(mNode01);
        mNode1Children = new ArrayList<>();
        mNode1Children.add(mNode10);
        mNode1Children.add(mNode11);
        mNode11Children = new ArrayList<>();
        mNode11Children.add(mNode110);
        mNode11Children.add(mNode111);

        mChildLocator = new ChildIndexUi2Locator(0);
        mGrandChildLocator = new ChildIndexUi2Locator(1, 1);

        when(mDevice.findObjects(argThat(matchesByDepth(0)))).thenReturn(mNodeList);
        when(mNode0.getChildren()).thenReturn(mNode0Children);
        when(mNode1.getChildren()).thenReturn(mNode1Children);
        when(mNode11.getChildren()).thenReturn(mNode11Children);
    }

    @Test
    public void locateOneDeviceChild() {
        UiObject2 result = mChildLocator.locateOne(mDevice);
        assertEquals(mNode0, result);
    }

    @Test
    public void locateOneDeviceGrandChild() {
        UiObject2 result = mGrandChildLocator.locateOne(mDevice);
        assertEquals(mNode11, result);
    }

    @Test
    public void locateAllDeviceChild() {
        List<UiObject2> results = mChildLocator.locateAll(mDevice);
        assertEquals(Collections.singletonList(mNode0), results);
    }

    @Test
    public void locateAllDeviceGrandChild() {
        List<UiObject2> results = mGrandChildLocator.locateAll(mDevice);
        assertEquals(Collections.singletonList(mNode11), results);
    }

    @Test
    public void locateOneNodeChild() {
        UiObject2 result = mChildLocator.locateOne(mNode0);
        assertEquals(mNode00, result);
    }

    @Test
    public void locateOneNodeGrandChild() {
        UiObject2 result = mGrandChildLocator.locateOne(mNode1);
        assertEquals(mNode111, result);
    }

    @Test
    public void locateAllNodeChild() {
        List<UiObject2> results = mChildLocator.locateAll(mNode0);
        assertEquals(Collections.singletonList(mNode00), results);
    }

    @Test
    public void locateAllNodeGrandChild() {
        List<UiObject2> results = mGrandChildLocator.locateAll(mNode1);
        assertEquals(Collections.singletonList(mNode111), results);
    }
}
