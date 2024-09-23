// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit tests for {@link PwaRestoreBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public final class PwaRestoreBottomSheetContentTest {
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    public void testBasics() {
        // Create a mocked version of the PwaRestoreBottomSheetView, for use with the
        // BottomSheetContent. Note that the view is not initialized, and therefore it does not
        // inflate its layout. That means attempts to access the underlying content view are not
        // likely to work.
        PwaRestoreBottomSheetView mockedView =
                Mockito.mock(
                        PwaRestoreBottomSheetView.class,
                        Mockito.withSettings()
                                .useConstructor(mActivity)
                                .defaultAnswer(Mockito.RETURNS_MOCKS));
        PwaRestoreBottomSheetContent pwaRestoreBottomSheetContent =
                new PwaRestoreBottomSheetContent(
                        (PwaRestoreBottomSheetView) mockedView, /* onOsBackButtonClicked= */ null);

        Assert.assertTrue(pwaRestoreBottomSheetContent.getContentView() != null);
        Assert.assertTrue(pwaRestoreBottomSheetContent.getToolbarView() == null);

        Assert.assertEquals(
                BottomSheetContent.ContentPriority.LOW, pwaRestoreBottomSheetContent.getPriority());
        pwaRestoreBottomSheetContent.setPriority(BottomSheetContent.ContentPriority.HIGH);
        Assert.assertEquals(
                BottomSheetContent.ContentPriority.HIGH,
                pwaRestoreBottomSheetContent.getPriority());

        Assert.assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                pwaRestoreBottomSheetContent.getPeekHeight(),
                0.0001);
        Assert.assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                pwaRestoreBottomSheetContent.getHalfHeightRatio(),
                0.0001);
        Assert.assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                pwaRestoreBottomSheetContent.getFullHeightRatio(),
                0.0001);
        Assert.assertEquals(0, pwaRestoreBottomSheetContent.getVerticalScrollOffset());
        Assert.assertFalse(pwaRestoreBottomSheetContent.swipeToDismissEnabled());

        int accessibilityId = R.string.pwa_restore_bottom_sheet_accessibility;
        Assert.assertEquals(
                accessibilityId, pwaRestoreBottomSheetContent.getSheetContentDescriptionStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaRestoreBottomSheetContent.getSheetHalfHeightAccessibilityStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaRestoreBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaRestoreBottomSheetContent.getSheetClosedAccessibilityStringId());
    }
}
