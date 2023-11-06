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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests for BySelectorUi2Locator */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class BySelectorUi2LocatorTest {
    private BySelector mSelector;

    @Mock private UiDevice mDevice;

    @Mock private UiObject2 mRoot;

    @Mock private UiObject2 mResult0;

    @Mock private UiObject2 mResult1;

    private List<UiObject2> mResults;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mSelector = By.res("resource");

        mResults = new ArrayList<>();
        mResults.add(mResult0);
        mResults.add(mResult1);

        TestUtils.stubMocks(mDevice, mRoot, mSelector, mResult0, mResults);
    }

    @Test
    public void locate() {
        BySelectorUi2Locator locator = new BySelectorUi2Locator(mSelector);
        TestUtils.assertLocatorResults(mDevice, mRoot, locator, mResult0, mResults);
    }
}
