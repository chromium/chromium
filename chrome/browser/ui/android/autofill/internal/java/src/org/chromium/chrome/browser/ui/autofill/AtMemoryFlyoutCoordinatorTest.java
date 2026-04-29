// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

/**
 * Component tests for the AtMemory Flyout Coordinator.
 */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryFlyoutCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AtMemoryFlyoutCoordinator.Delegate mMockDelegate;

    private AtMemoryFlyoutCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator =
                new AtMemoryFlyoutCoordinator(
                        ApplicationProvider.getApplicationContext(),
                        mBottomSheetController,
                        mMockDelegate);
    }

    @Test
    public void testInitialization() {
        assertNotNull(mCoordinator);
    }

    @Test
    public void testShowRequestsBottomSheet() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);

        mCoordinator.show();

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testShowFailsAndDismisses() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);

        mCoordinator.show();

        verify(mMockDelegate).onDismissed();
    }
}
