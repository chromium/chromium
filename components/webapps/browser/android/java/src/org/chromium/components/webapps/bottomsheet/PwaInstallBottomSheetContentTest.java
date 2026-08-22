// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.app.Activity;
import android.view.View;

import androidx.core.widget.NestedScrollView;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.webapps.R;

/** Unit tests for {@link PwaInstallBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class PwaInstallBottomSheetContentTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    @Test
    @MediumTest
    public void testBasicsWithMockedView() {
        PwaInstallBottomSheetView mockedView = Mockito.mock(PwaInstallBottomSheetView.class);
        Mockito.when(mockedView.getVerticalScrollOffset()).thenReturn(42);

        PwaInstallBottomSheetContent content = new PwaInstallBottomSheetContent(mockedView);

        Assert.assertEquals(42, content.getVerticalScrollOffset());
        Assert.assertEquals(BottomSheetContent.ContentPriority.LOW, content.getPriority());
        content.setPriority(BottomSheetContent.ContentPriority.HIGH);
        Assert.assertEquals(BottomSheetContent.ContentPriority.HIGH, content.getPriority());
        Assert.assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT, content.getFullHeightRatio(), 0.0001);
        Assert.assertTrue(content.swipeToDismissEnabled());

        int accessibilityId = R.string.pwa_install_bottom_sheet_accessibility;
        Assert.assertEquals(accessibilityId, content.getSheetHalfHeightAccessibilityStringId());
        Assert.assertEquals(accessibilityId, content.getSheetFullHeightAccessibilityStringId());
        Assert.assertEquals(accessibilityId, content.getSheetClosedAccessibilityStringId());
    }

    @Test
    @MediumTest
    public void testVerticalScrollOffsetWithRealView() {
        PwaBottomSheetController.ScreenshotsAdapter adapter =
                new PwaBottomSheetController.ScreenshotsAdapter(
                        mActivity, /* shouldPadForDialogContent= */ false);
        PwaInstallBottomSheetView view = new PwaInstallBottomSheetView(mActivity, adapter);
        PwaInstallBottomSheetContent content = new PwaInstallBottomSheetContent(view);

        Assert.assertNotNull(content.getContentView());
        Assert.assertNull(content.getToolbarView());

        // Initially scroll offset is 0.
        Assert.assertEquals(0, view.getVerticalScrollOffset());
        Assert.assertEquals(0, content.getVerticalScrollOffset());

        NestedScrollView scrollView = view.getContentView().findViewById(R.id.scroll_view);
        Assert.assertNotNull(scrollView);

        // Measure and layout the scroll view and its content so NestedScrollView allows scrolling.
        scrollView.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        scrollView.layout(0, 0, 500, 200);
        View child = scrollView.getChildAt(0);
        Assert.assertNotNull(child);
        child.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        child.layout(0, 0, 500, 1000);

        scrollView.scrollTo(0, 120);
        Assert.assertEquals(120, view.getVerticalScrollOffset());
        Assert.assertEquals(120, content.getVerticalScrollOffset());

        scrollView.scrollTo(0, 0);
        Assert.assertEquals(0, view.getVerticalScrollOffset());
        Assert.assertEquals(0, content.getVerticalScrollOffset());
    }
}
