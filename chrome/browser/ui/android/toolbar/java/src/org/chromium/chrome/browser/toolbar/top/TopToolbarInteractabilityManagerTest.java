// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Robolectric tests for {@link TopToolbarInteractabilityManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TopToolbarInteractabilityManagerTest {
    @Mock
    private TopToolbarInteractabilityManager.Delegate mDelegate;
    private TopToolbarInteractabilityManager mTopToolbarInteractabilityManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTopToolbarInteractabilityManager = new TopToolbarInteractabilityManager(mDelegate);
    }

    @Test
    @SmallTest
    public void testDelegateInvoked_WhenDisablingNewTabButton_Once() {
        int token = mTopToolbarInteractabilityManager.disableNewTabButton();
        verify(mDelegate, times(1)).setNewTabButtonEnabled(false);
    }

    @Test
    @SmallTest
    public void testDelegateNotInvoked_WhenDisablingNewTabButton_MoreThanOnceInRow() {
        int token1 = mTopToolbarInteractabilityManager.disableNewTabButton();
        verify(mDelegate, times(1)).setNewTabButtonEnabled(false);
        verifyNoMoreInteractions(mDelegate);

        int token2 = mTopToolbarInteractabilityManager.disableNewTabButton();
        assertNotEquals(token1, token2);
    }

    @Test
    @SmallTest
    public void testDelegateInvoked_WhenDisablingAndEnablingNewTabButton() {
        int token = mTopToolbarInteractabilityManager.disableNewTabButton();
        verify(mDelegate, times(1)).setNewTabButtonEnabled(false);

        mTopToolbarInteractabilityManager.enableNewTabButton(token);
        verify(mDelegate, times(1)).setNewTabButtonEnabled(true);
    }

    @Test
    @SmallTest
    public void testDelegateNotInvoked_WhenEnablingNewTabButton_BeforeDisabling() {
        mTopToolbarInteractabilityManager.enableNewTabButton(/*clientToken=*/1234);
        verifyZeroInteractions(mDelegate);
    }
}
