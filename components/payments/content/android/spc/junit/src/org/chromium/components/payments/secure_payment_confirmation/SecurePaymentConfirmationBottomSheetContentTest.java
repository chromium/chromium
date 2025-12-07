// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.secure_payment_confirmation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.ScrollView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.payments.R;

/** Unit tests for {@link SecurePaymentConfirmationBottomSheetContent} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SecurePaymentConfirmationBottomSheetContentTest {
    private Activity mActivity;
    private View mContentView;
    private ScrollView mScrollView;
    private SecurePaymentConfirmationBottomSheetContent mContent;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mContentView =
                mActivity.getLayoutInflater().inflate(R.layout.secure_payment_confirmation, null);
        mScrollView = mContentView.findViewById(R.id.scroll_view);
        mContent = new SecurePaymentConfirmationBottomSheetContent(mContentView, mScrollView);
    }

    @Test
    public void testContentView() {
        assertEquals(mContentView, mContent.getContentView());
    }

    @Test
    public void testNoToolbarView() {
        assertNull(mContent.getToolbarView());
    }

    @Test
    public void testVerticalScrollOffset() {
        mScrollView.setPadding(/* left= */ 0, /* top= */ 0, /* right= */ 0, /* bottom= */ 100);
        mScrollView.setScrollY(24);

        assertEquals(24, mContent.getVerticalScrollOffset());
    }

    @Test
    public void testVerticalScrollOffset_whenNotSet() {
        assertEquals(0, mContent.getVerticalScrollOffset());
    }

    @Test
    public void testFullHeightRatio() {
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mContent.getFullHeightRatio(),
                /* delta= */ 0);
    }

    @Test
    public void testHalfHeightRatio() {
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mContent.getHalfHeightRatio(),
                /* delta= */ 0);
    }

    @Test
    public void testCustomLifecycle() {
        assertFalse(mContent.hasCustomLifecycle());
    }

    @Test
    public void testSwipeToDismissEnabled() {
        assertTrue(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testPriority() {
        assertEquals(BottomSheetContent.ContentPriority.HIGH, mContent.getPriority());
    }

    @Test
    public void testPeekHeight() {
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED, mContent.getPeekHeight(), /* delta= */ 0);
    }

    @Test
    public void testSheetContentDescription() {
        assertEquals(
                mActivity.getString(
                        R.string.secure_payment_confirmation_authentication_sheet_description),
                mContent.getSheetContentDescription(mActivity));
    }

    @Test
    public void testSheetFullHeightAccessibilityString() {
        assertEquals(
                R.string.secure_payment_confirmation_authentication_sheet_opened,
                mContent.getSheetFullHeightAccessibilityStringId());
    }

    @Test
    public void testSheetClosedAccessibilityString() {
        assertEquals(
                R.string.secure_payment_confirmation_authentication_sheet_closed,
                mContent.getSheetClosedAccessibilityStringId());
    }
}
