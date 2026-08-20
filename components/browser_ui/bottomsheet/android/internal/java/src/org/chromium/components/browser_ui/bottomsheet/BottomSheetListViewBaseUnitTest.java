// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;

import java.util.Collections;
import java.util.Set;

/** Robolectric unit tests for {@link BottomSheetListViewBase}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomSheetListViewBaseUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;

    private static class TestBottomSheetListView extends BottomSheetListViewBase {
        private @Px int mDesiredHeight = 300;
        private @Px int mMaxHeight = 600;

        TestBottomSheetListView(BottomSheetController bottomSheetController, View contentView) {
            super(bottomSheetController, contentView, /* suppressCollectionA11y= */ false);
        }

        void setHeightsForTesting(@Px int desiredHeight, @Px int maxHeight) {
            mDesiredHeight = desiredHeight;
            mMaxHeight = maxHeight;
        }

        @Override
        protected View getHandlebar() {
            return getContentView();
        }

        @Override
        protected View getHeaderView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public @Px int getDesiredSheetHeightPx() {
            return mDesiredHeight;
        }

        @Override
        public @Px int getMaximumSheetHeightPx() {
            return mMaxHeight;
        }

        @Override
        protected @Px int getConclusiveMarginHeightPx() {
            return 0;
        }

        @Override
        protected @Px int getSideMarginPx() {
            return 0;
        }

        @Override
        protected Set<Integer> listedItemTypes() {
            return Collections.emptySet();
        }

        @Override
        protected Set<Integer> footerItemTypes() {
            return Collections.emptySet();
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return android.R.string.ok;
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return android.R.string.ok;
        }
    }

    private TestBottomSheetListView mListViewBase;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = new View(activity);
        mListViewBase = new TestBottomSheetListView(mMockBottomSheetController, contentView);
    }

    @Test
    public void testHeightRatios_StandardMode() {
        // In standard mode, getMaxSheetHeight() == getContainerHeight() == 1000.
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1000);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(1000);
        mListViewBase.setHeightsForTesting(300, 600);

        assertEquals(0.6f, mListViewBase.getFullHeightRatio(), 0.001f);
        assertEquals(0.3f, mListViewBase.getHalfHeightRatio(), 0.001f);
    }

    @Test
    public void testHeightRatios_LargeFormFactor() {
        // In LFF mode, container is 1000px, but maxSheetHeight is 800px due to margins/top gap.
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(800);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(1000);
        mListViewBase.setHeightsForTesting(300, 600);

        // Ratios must be normalized by maxSheetHeight (800) so that:
        // fullHeight = 600 / 800 * 800 = 600px
        // halfHeight = 300 / 800 * 800 = 300px
        assertEquals(600f / 800f, mListViewBase.getFullHeightRatio(), 0.001f);
        assertEquals(300f / 800f, mListViewBase.getHalfHeightRatio(), 0.001f);
    }

    @Test
    public void testHeightRatios_FallbackToContainerHeightWhenMaxSheetHeightZero() {
        // If maxSheetHeight is 0 (e.g. uninitialized sheet), fallback to containerHeight.
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(0);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(1000);
        mListViewBase.setHeightsForTesting(300, 600);

        assertEquals(0.6f, mListViewBase.getFullHeightRatio(), 0.001f);
        assertEquals(0.3f, mListViewBase.getHalfHeightRatio(), 0.001f);
    }

    @Test
    public void testHeightRatios_BothHeightsZeroOrNegative() {
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(0);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(0);

        assertEquals(HeightMode.DEFAULT, mListViewBase.getFullHeightRatio(), 0.001f);
        assertEquals(HeightMode.DISABLED, mListViewBase.getHalfHeightRatio(), 0.001f);
    }

    @Test
    public void testIsFullyExtended_UsesMaxSheetHeight() {
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(800);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(1000);
        mListViewBase.setHeightsForTesting(300, 600);

        when(mMockBottomSheetController.getCurrentOffset()).thenReturn(599);
        assertFalse(mListViewBase.isFullyExtended());

        when(mMockBottomSheetController.getCurrentOffset()).thenReturn(600);
        assertTrue(mListViewBase.isFullyExtended());
    }
}
