// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

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

/** Unit tests for {@link PwaUniversalInstallBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.PAUSED)
public final class PwaUniversalInstallBottomSheetContentTest {
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @MediumTest
    public void testBasics() {
        // Create a mocked version of the PwaUniversalInstallBottomSheetView, for use with the
        // BottomSheetContent. Note that the view is not initialized, and therefore it does not
        // inflate its layout. That means attempts to access the underlying content view are not
        // likely to work.
        PwaUniversalInstallBottomSheetView mockedView =
                Mockito.mock(
                        PwaUniversalInstallBottomSheetView.class,
                        Mockito.withSettings()
                                .useConstructor()
                                .defaultAnswer(Mockito.RETURNS_MOCKS));
        PwaUniversalInstallBottomSheetContent pwaUniversalInstallBottomSheetContent =
                new PwaUniversalInstallBottomSheetContent(
                        (PwaUniversalInstallBottomSheetView) mockedView,
                        /* recordBackButtonCallback= */ null);

        Assert.assertTrue(pwaUniversalInstallBottomSheetContent.getContentView() != null);
        Assert.assertTrue(pwaUniversalInstallBottomSheetContent.getToolbarView() == null);

        Assert.assertEquals(
                BottomSheetContent.ContentPriority.HIGH,
                pwaUniversalInstallBottomSheetContent.getPriority());
        pwaUniversalInstallBottomSheetContent.setPriority(BottomSheetContent.ContentPriority.LOW);
        Assert.assertEquals(
                BottomSheetContent.ContentPriority.LOW,
                pwaUniversalInstallBottomSheetContent.getPriority());

        Assert.assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                pwaUniversalInstallBottomSheetContent.getFullHeightRatio(),
                0.0001);
        Assert.assertEquals(0, pwaUniversalInstallBottomSheetContent.getVerticalScrollOffset());
        Assert.assertTrue(pwaUniversalInstallBottomSheetContent.swipeToDismissEnabled());

        int accessibilityId = R.string.pwa_uni_bottom_sheet_accessibility;
        Assert.assertEquals(
                accessibilityId,
                pwaUniversalInstallBottomSheetContent.getSheetContentDescriptionStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaUniversalInstallBottomSheetContent.getSheetHalfHeightAccessibilityStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaUniversalInstallBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        Assert.assertEquals(
                accessibilityId,
                pwaUniversalInstallBottomSheetContent.getSheetClosedAccessibilityStringId());
    }
}
