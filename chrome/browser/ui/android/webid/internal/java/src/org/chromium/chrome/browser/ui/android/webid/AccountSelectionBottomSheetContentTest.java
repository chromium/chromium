// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertFalse;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** JUnit tests for {@link AccountSelectionBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionBottomSheetContentTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetContent mLowestPriorityContent;
    @Mock private BottomSheetContent mLowerPriorityContent;
    @Mock private BottomSheetContent mSamePriorityContent;
    @Mock private View mView;
    @Mock private BottomSheetController mBottomSheetController;
    private AccountSelectionBottomSheetContent mContent;

    @Before
    public void setUp() {
        mContent =
                new AccountSelectionBottomSheetContent(
                        /* contentView= */ mView,
                        /* bottomSheetController= */ mBottomSheetController,
                        /* scrollOffsetSupplier= */ () -> 0,
                        RpMode.PASSIVE);
    }

    @Test
    public void testCanBeSuppressed() {
        Mockito.when(mLowestPriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.COBROWSE);

        Mockito.when(mLowerPriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.LOW);

        Mockito.when(mSamePriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.HIGH);

        // Same (HIGH = 0) or lower priority (LOW = 1) or lowest priority (COBROWSE = 2) content
        // cannot suppress this content
        assertFalse(mContent.canBeSuppressed(mSamePriorityContent));
        assertFalse(mContent.canBeSuppressed(mLowerPriorityContent));
        assertFalse(mContent.canBeSuppressed(mLowestPriorityContent));
    }
}
