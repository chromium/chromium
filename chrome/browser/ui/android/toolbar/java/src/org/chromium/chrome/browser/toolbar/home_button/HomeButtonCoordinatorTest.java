// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.home_button;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for HomeButtonCoordinator. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeButtonCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private HomeButton mHomeButton;
    @Mock private android.content.res.Resources mResources;

    private boolean mIsHomeButtonMenuDisabled;
    private HomeButtonCoordinator mHomeButtonCoordinator;

    @Before
    public void setUp() {
        when(mHomeButton.getRootView()).thenReturn(Mockito.mock(View.class));
        when(mHomeButton.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(LayoutInflater.from(ContextUtils.getApplicationContext()));

        mIsHomeButtonMenuDisabled = false;
        mHomeButtonCoordinator =
                new HomeButtonCoordinator(
                        mContext,
                        mHomeButton,
                        (context) -> {},
                        () -> mIsHomeButtonMenuDisabled);
    }

    @Test
    public void testListMenu() {
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton).showMenu();
        assertEquals(1, mHomeButtonCoordinator.getMenuForTesting().size());
    }

    @Test
    public void testListMenuDisabled() {
        mIsHomeButtonMenuDisabled = true;
        mHomeButtonCoordinator.onLongClickHomeButton(mHomeButton);

        verify(mHomeButton, never()).showMenu();
    }
}
