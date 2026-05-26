// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/** Unit tests for {@link AtMemoryBottomSheetCoordinator}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryBottomSheetCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AtMemoryBottomSheetCoordinator.Delegate mMockDelegate;

    private AtMemoryBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator =
                new AtMemoryBottomSheetCoordinator(
                        new ContextThemeWrapper(
                                ApplicationProvider.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight),
                        mBottomSheetController,
                        mMockDelegate);
    }

    @Test
    public void testInitialization() {
        assertNotNull(mCoordinator);
    }

    @Test
    public void testShow_Success() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(true);

        mCoordinator.show(List.of());

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    public void testShow_Failed() {
        when(mBottomSheetController.requestShowContent(any(), eq(true))).thenReturn(false);

        mCoordinator.show(List.of());

        verify(mMockDelegate).onDismissed();
    }

    @Test
    public void testHide() {
        mCoordinator.hide();

        verify(mBottomSheetController).hideContent(any(), eq(true));
    }
}
