// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static android.view.View.MeasureSpec.UNSPECIFIED;
import static android.view.View.MeasureSpec.makeMeasureSpec;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.toolbar.ToolbarWidthConsumer;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link OmniboxChipManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxChipManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityController<TestActivity> mActivityController;
    private OmniboxChipManager mManager;
    private FrameLayout mRootView;
    private ToolbarWidthConsumer mCollapsedConsumer;
    private ToolbarWidthConsumer mExpandedConsumer;
    private Drawable mIcon;
    @Mock private OmniboxChipManager.ChipCallback mCallback;
    @Mock private LocationBarEmbedder mLocationBarEmbedder;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class);
        var activity = mActivityController.setup().get();
        mRootView = new FrameLayout(activity);
        ((ViewGroup) activity.findViewById(android.R.id.content)).addView(mRootView);
        mManager = new OmniboxChipManager(mRootView, mLocationBarEmbedder);
        mCollapsedConsumer = mManager.getCollapsedToolbarWidthConsumer();
        mExpandedConsumer = mManager.getExpandedToolbarWidthConsumer();
        mIcon = activity.getDrawable(R.drawable.ic_open_in_new_20dp);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void placeChip_shownCollapsed() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);
        assertTrue(mManager.isChipPlaced());
        assertEquals(View.VISIBLE, mRootView.getVisibility());
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();

        {
            int available = mManager.getCollapsedWidthForTesting() + 10;
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(mManager.getCollapsedWidthForTesting(), used);
            assertTrue(mCollapsedConsumer.isVisible());
            verify(mCallback).onChipShown();
        }

        {
            // A little less available than what we need.
            int available =
                    mManager.getMinExpandedWidthForTesting()
                            - mManager.getCollapsedWidthForTesting()
                            - 1;
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertFalse(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }
    }

    @Test
    public void placeChip_notShown() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);
        // Even if the chip isn't currently visible on the toolbar, it's still shown.
        assertTrue(mManager.isChipPlaced());

        {
            int available = mManager.getCollapsedWidthForTesting() - 10;
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertFalse(mCollapsedConsumer.isVisible());
            // Never been shown, so we don't call the callback.
            verify(mCallback, never()).onChipHidden();
        }

        {
            int available = 0;
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(0, used);
            assertFalse(mExpandedConsumer.isVisible());
            verify(mCallback, never()).onChipHidden();
        }
    }

    @Test
    public void placeChip_shownCollapsedThenHidden() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);

        {
            int available = mManager.getCollapsedWidthForTesting() + 10;
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(mManager.getCollapsedWidthForTesting(), used);
            assertTrue(mCollapsedConsumer.isVisible());
            verify(mCallback).onChipShown();
        }

        {
            // A little less available than what we need.
            int available =
                    mManager.getMinExpandedWidthForTesting()
                            - mManager.getCollapsedWidthForTesting()
                            - 1;
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertFalse(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }

        {
            int available = mManager.getCollapsedWidthForTesting() - 10;
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertFalse(mCollapsedConsumer.isVisible());
            verify(mCallback).onChipHidden();
        }

        {
            int available = 0;
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(0, used);
            assertFalse(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipHidden();
        }
    }

    @Test
    public void placeChip_shownExpanded() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);

        {
            int available = mManager.getMinExpandedWidthForTesting();
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(mManager.getCollapsedWidthForTesting(), used);
            assertTrue(mCollapsedConsumer.isVisible());
            verify(mCallback).onChipShown();
        }

        {
            // Enough to show expanded.
            int available =
                    mManager.getMinExpandedWidthForTesting()
                            - mManager.getCollapsedWidthForTesting();
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertTrue(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }
    }

    @Test
    public void placeChip_shownExpandedThenCollapsed() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);

        {
            int available = mManager.getMinExpandedWidthForTesting();
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(mManager.getCollapsedWidthForTesting(), used);
            assertTrue(mCollapsedConsumer.isVisible());
            verify(mCallback).onChipShown();
        }

        {
            // Enough to show expanded.
            int available =
                    mManager.getMinExpandedWidthForTesting()
                            - mManager.getCollapsedWidthForTesting();
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(available, used);
            assertTrue(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }

        {
            int available = mManager.getCollapsedWidthForTesting() + 10;
            int used =
                    mCollapsedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(mManager.getCollapsedWidthForTesting(), used);
            assertTrue(mCollapsedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }

        {
            int available = 0;
            int used =
                    mExpandedConsumer.updateVisibility(
                            available,
                            makeMeasureSpec(0, UNSPECIFIED),
                            makeMeasureSpec(0, UNSPECIFIED));
            assertEquals(0, used);
            assertFalse(mExpandedConsumer.isVisible());
            // Callback shouldn't be called again.
            verify(mCallback, times(1)).onChipShown();
        }

        // Chip was never hidden.
        verify(mCallback, never()).onChipHidden();
    }

    @Test
    public void dismissChip() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);
        assertTrue(mManager.isChipPlaced());
        verify(mLocationBarEmbedder).onWidthConsumerVisibilityChanged();
        mManager.dismissChip();
        assertFalse(mManager.isChipPlaced());
        assertEquals(View.GONE, mRootView.getVisibility());
        assertFalse(mCollapsedConsumer.isVisible());
        assertFalse(mExpandedConsumer.isVisible());
        onView(withText("text")).check(doesNotExist());
        verify(mLocationBarEmbedder, times(2)).onWidthConsumerVisibilityChanged();
    }

    @Test
    public void updateChip() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);
        onView(withText("text")).check(matches(isDisplayed()));

        mManager.placeChip("other text", mIcon, "other contentDesc", () -> {}, mCallback);
        onView(withText("other text")).check(matches(isDisplayed()));
    }

    @Test
    public void omniboxFocused() {
        mManager.placeChip("text", mIcon, "contentDesc", () -> {}, mCallback);
        onView(withText("text")).check(matches(isDisplayed()));

        mManager.setOmniboxFocused(true);
        assertEquals(View.INVISIBLE, mRootView.getVisibility());

        mManager.setOmniboxFocused(false);
        assertEquals(View.VISIBLE, mRootView.getVisibility());

        mManager.dismissChip();
        assertEquals(View.GONE, mRootView.getVisibility());
    }
}
