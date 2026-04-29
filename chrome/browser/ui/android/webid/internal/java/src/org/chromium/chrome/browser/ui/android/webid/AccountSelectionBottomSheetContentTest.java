// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** JUnit tests for {@link AccountSelectionBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccountSelectionBottomSheetContentTest {
    private AccountSelectionBottomSheetContent mContent;

    @Before
    public void setUp() {
        mContent =
                new AccountSelectionBottomSheetContent(
                        /* contentView= */ Mockito.mock(View.class),
                        /* bottomSheetController= */ Mockito.mock(BottomSheetController.class),
                        /* scrollOffsetSupplier= */ () -> 0,
                        RpMode.PASSIVE);
    }

    @Test
    public void testCanBeSuppressed() {
        BottomSheetContent higherPriorityContent = Mockito.mock(BottomSheetContent.class);
        Mockito.when(higherPriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.COBROWSE);

        BottomSheetContent lowerPriorityContent = Mockito.mock(BottomSheetContent.class);
        Mockito.when(lowerPriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.LOW);

        BottomSheetContent samePriorityContent = Mockito.mock(BottomSheetContent.class);
        Mockito.when(samePriorityContent.getPriority())
                .thenReturn(BottomSheetContent.ContentPriority.HIGH);

        // High priority content (COBROWSE = 0) can suppress this content (HIGH = 1)
        assertTrue(mContent.canBeSuppressed(higherPriorityContent));

        // Same (HIGH = 1) or lower priority (LOW = 2) content cannot suppress this content
        assertFalse(mContent.canBeSuppressed(samePriorityContent));
        assertFalse(mContent.canBeSuppressed(lowerPriorityContent));
    }
}
