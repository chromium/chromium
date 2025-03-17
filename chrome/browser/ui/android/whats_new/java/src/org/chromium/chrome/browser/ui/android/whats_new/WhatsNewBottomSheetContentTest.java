// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link WhatsNewBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class WhatsNewBottomSheetContentTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BottomSheetController mBottomSheetControllerMock;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    @Test
    @MediumTest
    public void testBasics() {
        View testView = new View(mActivity);

        WhatsNewBottomSheetContent whatsNewBottomSheetContent =
                new WhatsNewBottomSheetContent(testView, mBottomSheetControllerMock, null);

        assertTrue(whatsNewBottomSheetContent.getContentView() != null);
        assertTrue(whatsNewBottomSheetContent.getToolbarView() == null);

        assertEquals(
                BottomSheetContent.ContentPriority.HIGH, whatsNewBottomSheetContent.getPriority());

        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                whatsNewBottomSheetContent.getPeekHeight(),
                0.0001);
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                whatsNewBottomSheetContent.getHalfHeightRatio(),
                0.0001);
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                whatsNewBottomSheetContent.getFullHeightRatio(),
                0.0001);
        assertEquals(0, whatsNewBottomSheetContent.getVerticalScrollOffset());
        assertFalse(whatsNewBottomSheetContent.swipeToDismissEnabled());

        assertEquals(
                mActivity.getString(R.string.whats_new_page_description),
                whatsNewBottomSheetContent.getSheetContentDescription(mActivity));
        assertEquals(
                R.string.whats_new_page_title,
                whatsNewBottomSheetContent.getSheetHalfHeightAccessibilityStringId());
        assertEquals(
                R.string.whats_new_page_title,
                whatsNewBottomSheetContent.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.whats_new_page_title,
                whatsNewBottomSheetContent.getSheetClosedAccessibilityStringId());
    }
}
