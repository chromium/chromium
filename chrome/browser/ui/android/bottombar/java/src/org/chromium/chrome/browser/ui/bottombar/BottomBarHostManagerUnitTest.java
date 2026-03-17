// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.bottombar.BottomBarHostManager.Host;

/** Unit tests for {@link BottomBarHostManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomBarHostManagerUnitTest {
    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BottomBar mBottomBar;
    @Mock private View mView;
    @Mock private ViewGroup mParentView;
    @Mock private Callback<View> mCallback;

    private BottomBarHostManager mHostManager;

    @Before
    public void setUp() {
        mHostManager = new BottomBarHostManager();
        when(mBottomBar.getView()).thenReturn(mView);
    }

    @Test
    public void testTakeOwnership() {
        mHostManager.registerBottomBar(mBottomBar);

        mHostManager.takeOwnership(Host.HUB, mCallback);

        verify(mCallback).onResult(mView);
        verify(mBottomBar).setParent(Host.HUB);
    }

    @Test
    public void testTakeOwnership_WithParent() {
        when(mView.getParent()).thenReturn(mParentView);
        mHostManager.registerBottomBar(mBottomBar);

        mHostManager.takeOwnership(Host.TABBED, mCallback);

        verify(mCallback).onResult(mView);
        verify(mParentView).removeView(mView);
        verify(mBottomBar).setParent(Host.TABBED);
    }

    @Test(expected = AssertionError.class)
    public void testRegisterBottomBarTwice() {
        mHostManager.registerBottomBar(mBottomBar);
        // This should trigger the assertion
        mHostManager.registerBottomBar(mBottomBar);
    }

    @Test(expected = AssertionError.class)
    public void testTakeOwnership_WithoutRegistration() {
        // This should trigger the assertion because mBottomBar is null
        mHostManager.takeOwnership(Host.HUB, mCallback);
    }
}
