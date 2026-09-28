// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Robolectric unit tests for {@link BottomSheetListViewBase}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetListViewBaseUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Callback<Integer> mMockDismissHandler;

    private static class TestBottomSheetListView extends BottomSheetListViewBase {
        private @Px int mDesiredHeight = 300;
        private @Px int mMaxHeight = 600;

        int mLastContainerWidth;
        int mLastContainerHeight;

        TestBottomSheetListView(BottomSheetController bottomSheetController, View contentView) {
            super(bottomSheetController, contentView, /* suppressCollectionA11y= */ false);
        }

        @Override
        protected void onContainerSizeChanged(@Px int width, @Px int height) {
            super.onContainerSizeChanged(width, height);
            mLastContainerWidth = width;
            mLastContainerHeight = height;
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
        // In standard mode the sheet can use the whole container.
        final int containerHeight = 1000;
        final int desiredHeight = 300;
        final int maxHeight = 600;
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(containerHeight);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(containerHeight);
        mListViewBase.setHeightsForTesting(desiredHeight, maxHeight);

        assertEquals(
                "Full height ratio should be the content's full height over the container height",
                (float) maxHeight / containerHeight,
                mListViewBase.getFullHeightRatio(),
                0.001f);
        assertEquals(
                "Half height ratio should be the content's half height over the container height",
                (float) desiredHeight / containerHeight,
                mListViewBase.getHalfHeightRatio(),
                0.001f);
    }

    @Test
    public void testHeightRatios_LargeFormFactor() {
        // On large screens the sheet is shorter than its container because of margins and the
        // gap at the top.
        final int containerHeight = 1000;
        final int maxSheetHeight = 800;
        final int desiredHeight = 300;
        final int maxHeight = 600;
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(maxSheetHeight);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(containerHeight);
        mListViewBase.setHeightsForTesting(desiredHeight, maxHeight);

        assertEquals(
                "Full height ratio should be measured against the sheet's own maximum height",
                (float) maxHeight / maxSheetHeight,
                mListViewBase.getFullHeightRatio(),
                0.001f);
        assertEquals(
                "Half height ratio should be measured against the sheet's own maximum height",
                (float) desiredHeight / maxSheetHeight,
                mListViewBase.getHalfHeightRatio(),
                0.001f);
    }

    @Test
    public void testHeightRatios_FallbackToContainerHeightWhenMaxSheetHeightZero() {
        // If the sheet's maximum height is not known yet (0), the container height is used.
        final int containerHeight = 1000;
        final int desiredHeight = 300;
        final int maxHeight = 600;
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(0);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(containerHeight);
        mListViewBase.setHeightsForTesting(desiredHeight, maxHeight);

        assertEquals(
                "Full height ratio should use the container height when the sheet height is 0",
                (float) maxHeight / containerHeight,
                mListViewBase.getFullHeightRatio(),
                0.001f);
        assertEquals(
                "Half height ratio should use the container height when the sheet height is 0",
                (float) desiredHeight / containerHeight,
                mListViewBase.getHalfHeightRatio(),
                0.001f);
    }

    @Test
    public void testHeightRatios_BothHeightsZeroOrNegative() {
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(0);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(0);

        assertEquals(
                "Full height ratio should be DEFAULT when both heights are 0",
                HeightMode.DEFAULT,
                mListViewBase.getFullHeightRatio(),
                0.001f);
        assertEquals(
                "Half height ratio should be DISABLED when both heights are 0",
                HeightMode.DISABLED,
                mListViewBase.getHalfHeightRatio(),
                0.001f);

        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(-5);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(-10);

        assertEquals(
                "Full height ratio should be DEFAULT when both heights are negative",
                HeightMode.DEFAULT,
                mListViewBase.getFullHeightRatio(),
                0.001f);
        assertEquals(
                "Half height ratio should be DISABLED when both heights are negative",
                HeightMode.DISABLED,
                mListViewBase.getHalfHeightRatio(),
                0.001f);
    }

    @Test
    public void testIsFullyExtended_UsesMaxSheetHeight() {
        // The content wants to be taller than the sheet can be, so the sheet's own maximum
        // height (not the content or the container) decides when it is fully extended.
        final int containerHeight = 1000;
        final int maxSheetHeight = 800;
        final int contentMaxHeight = 900;
        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(maxSheetHeight);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(containerHeight);
        mListViewBase.setHeightsForTesting(300, contentMaxHeight);

        when(mMockBottomSheetController.getCurrentOffset()).thenReturn(maxSheetHeight - 1);
        assertFalse(
                "Sheet should not be fully extended just below its maximum height",
                mListViewBase.isFullyExtended());

        when(mMockBottomSheetController.getCurrentOffset()).thenReturn(maxSheetHeight);
        assertTrue(
                "Sheet should be fully extended when it reaches its maximum height",
                mListViewBase.isFullyExtended());
    }

    @Test
    public void testOnSheetStateChanged_HalfState_StandardMode_SuppressesLayout() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        RecyclerView recyclerView = new RecyclerView(activity);
        mListViewBase.setSheetItemListView(recyclerView);

        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mListViewBase);
        when(mMockBottomSheetController.isLargeFormFactorUiEnabled(mListViewBase))
                .thenReturn(false);
        when(mMockBottomSheetController.requestShowContent(mListViewBase, true)).thenReturn(true);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        mListViewBase.setVisible(true);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());

        BottomSheetObserver observer = observerCaptor.getValue();
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        assertTrue(
                "Recycler layout should be suppressed in HALF state in standard mode",
                recyclerView.isLayoutSuppressed());
    }

    @Test
    public void testOnSheetStateChanged_HalfState_LargeFormFactor_DoesNotSuppressLayout() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        RecyclerView recyclerView = new RecyclerView(activity);
        mListViewBase.setSheetItemListView(recyclerView);

        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mListViewBase);
        when(mMockBottomSheetController.isLargeFormFactorUiEnabled(mListViewBase)).thenReturn(true);
        when(mMockBottomSheetController.requestShowContent(mListViewBase, true)).thenReturn(true);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        mListViewBase.setVisible(true);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());

        BottomSheetObserver observer = observerCaptor.getValue();
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);

        assertFalse(
                "Recycler layout should not be suppressed in HALF state in large form factor mode",
                recyclerView.isLayoutSuppressed());
    }

    @Test
    public void testOnSheetStateChanged_HiddenState_UnsuppressesLayout() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        RecyclerView recyclerView = new RecyclerView(activity);
        mListViewBase.setSheetItemListView(recyclerView);

        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mListViewBase);
        when(mMockBottomSheetController.isLargeFormFactorUiEnabled(mListViewBase))
                .thenReturn(false);
        when(mMockBottomSheetController.requestShowContent(mListViewBase, true)).thenReturn(true);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        mListViewBase.setVisible(true);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());

        BottomSheetObserver observer = observerCaptor.getValue();
        observer.onSheetStateChanged(SheetState.HALF, StateChangeReason.NONE);
        assertTrue(
                "Recycler layout should be suppressed in HALF state",
                recyclerView.isLayoutSuppressed());

        observer.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);
        assertFalse(
                "Recycler layout should not be suppressed after transitioning to HIDDEN state",
                recyclerView.isLayoutSuppressed());
    }

    private static class RealMeasuringBottomSheetListView extends BottomSheetListViewBase {
        private View mHandlebar;
        private @Nullable View mHeaderView;
        private @Px int mConclusiveMarginHeightPx;
        private Set<Integer> mListedItemTypes = Set.of(0);
        private Set<Integer> mFooterItemTypes = Collections.emptySet();

        RealMeasuringBottomSheetListView(
                BottomSheetController bottomSheetController, View contentView) {
            super(bottomSheetController, contentView, /* suppressCollectionA11y= */ false);
            mHandlebar = contentView;
        }

        void setHandlebarForTesting(View handlebar) {
            mHandlebar = handlebar;
        }

        void setHeaderViewForTesting(@Nullable View headerView) {
            mHeaderView = headerView;
        }

        void setConclusiveMarginHeightPxForTesting(@Px int conclusiveMarginHeightPx) {
            mConclusiveMarginHeightPx = conclusiveMarginHeightPx;
        }

        void setListedItemTypesForTesting(Set<Integer> listedItemTypes) {
            mListedItemTypes = listedItemTypes;
        }

        void setFooterItemTypesForTesting(Set<Integer> footerItemTypes) {
            mFooterItemTypes = footerItemTypes;
        }

        @Override
        protected View getHandlebar() {
            return mHandlebar;
        }

        @Override
        protected @Nullable View getHeaderView() {
            return mHeaderView;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        protected @Px int getConclusiveMarginHeightPx() {
            return mConclusiveMarginHeightPx;
        }

        @Override
        protected @Px int getSideMarginPx() {
            return 0;
        }

        @Override
        protected Set<Integer> listedItemTypes() {
            return mListedItemTypes;
        }

        @Override
        protected Set<Integer> footerItemTypes() {
            return mFooterItemTypes;
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

    @Test
    public void testMeasurementCaching_ReturnsCachedHeightWhileScrolled() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = new View(activity);
        contentView.measure(
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        contentView.layout(0, 0, 800, 1000);

        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        int[] scrollOffset = new int[] {0};
        RecyclerView recyclerView =
                new RecyclerView(activity) {
                    @Override
                    public int computeVerticalScrollOffset() {
                        return scrollOffset[0];
                    }
                };
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 800, 500);

        RecyclerView.Adapter adapter =
                new RecyclerView.Adapter() {
                    @Override
                    public RecyclerView.ViewHolder onCreateViewHolder(
                            android.view.ViewGroup parent, int viewType) {
                        return new RecyclerView.ViewHolder(new View(activity)) {};
                    }

                    @Override
                    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

                    @Override
                    public int getItemCount() {
                        return 5;
                    }

                    @Override
                    public int getItemViewType(int position) {
                        return 0;
                    }
                };

        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        when(mMockBottomSheetController.getMaxSheetHeight()).thenReturn(1000);
        when(mMockBottomSheetController.getContainerHeight()).thenReturn(1000);

        // Initial measurement at top (offset = 0) computes and caches height.
        int initialDesiredHeight = listView.getDesiredSheetHeightPx();
        int initialMaxHeight = listView.getMaximumSheetHeightPx();

        // Simulate scrolling down: offset > 0.
        scrollOffset[0] = 200;
        listView.getScrollListenerForTesting().onScrolled(recyclerView, 0, 200);

        // While scrolled, querying desired and max height must return the cached values.
        assertEquals(initialDesiredHeight, listView.getDesiredSheetHeightPx());
        assertEquals(initialMaxHeight, listView.getMaximumSheetHeightPx());

        // Invalidate cache (e.g. on dataset update) -> recomputes fresh.
        listView.invalidateMeasurementCache();
        // Since still scrolled, next measurement at top will recache.
    }

    @Test
    public void testLayoutDirection_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;
        View contentView = new View(activity);
        TestBottomSheetListView listView =
                new TestBottomSheetListView(mMockBottomSheetController, contentView);
        assertEquals(View.LAYOUT_DIRECTION_RTL, listView.getContentView().getLayoutDirection());
    }

    @Test
    public void testLayoutDirection_Ltr() {
        LocalizationUtils.setRtlForTesting(false);
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.getApplicationInfo().flags |= ApplicationInfo.FLAG_SUPPORTS_RTL;
        View contentView = new View(activity);
        TestBottomSheetListView listView =
                new TestBottomSheetListView(mMockBottomSheetController, contentView);
        assertEquals(View.LAYOUT_DIRECTION_LTR, listView.getContentView().getLayoutDirection());
    }

    // Verifies that onContainerSizeChanged clears cached height measurements and propagates
    // updated container dimensions to subclasses.
    @Test
    public void testOnContainerSizeChanged_invalidatesMeasurementCacheAndNotifiesSubclass() {
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mListViewBase);

        // Pre-populate cached measurements to verify they get invalidated on container resize.
        mListViewBase.setCachedHeightsForTesting(300, 600);
        assertEquals(300, mListViewBase.getCachedDesiredSheetHeightPxForTesting());
        assertEquals(600, mListViewBase.getCachedMaximumSheetHeightPxForTesting());

        // Notify that the container size changed (e.g. orientation switch or window resize).
        mListViewBase.getBottomSheetObserverForTesting().onContainerSizeChanged(1200, 800);

        // Both desired and maximum sheet height caches should be reset to invalid dimension (-1).
        assertEquals(
                BottomSheetListViewBase.INVALID_PX_DIMENSION,
                mListViewBase.getCachedDesiredSheetHeightPxForTesting());
        assertEquals(
                BottomSheetListViewBase.INVALID_PX_DIMENSION,
                mListViewBase.getCachedMaximumSheetHeightPxForTesting());

        // Verify that the subclass received the updated container dimensions.
        assertEquals(1200, mListViewBase.mLastContainerWidth);
        assertEquals(800, mListViewBase.mLastContainerHeight);
    }

    // Verifies that container size changes are ignored when this sheet is not the active content.
    @Test
    public void testOnContainerSizeChanged_inactiveContent_doesNotInvalidateOrNotify() {
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(null);

        // Pre-populate cached measurements.
        mListViewBase.setCachedHeightsForTesting(300, 600);

        mListViewBase.getBottomSheetObserverForTesting().onContainerSizeChanged(1200, 800);

        // Cache must remain intact and subclass should not be notified.
        assertEquals(300, mListViewBase.getCachedDesiredSheetHeightPxForTesting());
        assertEquals(600, mListViewBase.getCachedMaximumSheetHeightPxForTesting());
        assertEquals(0, mListViewBase.mLastContainerWidth);
        assertEquals(0, mListViewBase.mLastContainerHeight);
    }

    /**
     * Test that `canDragSheet` safely returns false for null events, returns false when the list
     * view has not been initialized, is hidden, or when content view is hidden or has non-positive
     * width, and returns true only for touch coordinates strictly within the content view bounds
     * and above the list view.
     */
    @Test
    public void testCanDragSheet() {
        // When motion event is null, canDragSheet should safely return false.
        assertFalse(mListViewBase.canDragSheet(null));

        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 500, 250, 0);

        // When the list view has not been initialized, canDragSheet should return false.
        assertFalse(mListViewBase.canDragSheet(event));

        Activity activity = Robolectric.setupActivity(Activity.class);
        FrameLayout contentView = new FrameLayout(activity);
        RecyclerView recyclerView = new RecyclerView(activity);
        contentView.addView(recyclerView);

        TestBottomSheetListView listViewBase =
                new TestBottomSheetListView(mMockBottomSheetController, contentView);
        listViewBase.setSheetItemListView(recyclerView);

        // Attach contentView to the activity.
        activity.setContentView(contentView);

        // Layout contentView from x=[100, 900], y=[200, 1000].
        contentView.layout(100, 200, 900, 1000);
        // Layout recyclerView inside contentView below header from x=[0, 800], y=[150, 800].
        recyclerView.layout(0, 150, 800, 800);

        int[] listLocation = new int[2];
        recyclerView.getLocationOnScreen(listLocation);
        int listTop = listLocation[1];

        int[] contentLocation = new int[2];
        contentView.getLocationOnScreen(contentLocation);
        int contentLeft = contentLocation[0];
        int contentTop = contentLocation[1];
        int contentRight = contentLeft + contentView.getWidth();

        // Coordinates above contentView (e.g. in scrim area) should return false.
        MotionEvent eventAbove =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 500, contentTop - 10, 0);
        assertFalse(listViewBase.canDragSheet(eventAbove));

        // Coordinates to the left of contentView should return false.
        MotionEvent eventLeft =
                MotionEvent.obtain(
                        0, 0, MotionEvent.ACTION_DOWN, contentLeft - 10, contentTop + 50, 0);
        assertFalse(listViewBase.canDragSheet(eventLeft));

        // Coordinates to the right of contentView should return false.
        MotionEvent eventRight =
                MotionEvent.obtain(
                        0, 0, MotionEvent.ACTION_DOWN, contentRight + 10, contentTop + 50, 0);
        assertFalse(listViewBase.canDragSheet(eventRight));

        // Coordinates within header area (between contentTop and listTop) should return true.
        MotionEvent eventHeader =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 500, contentTop + 50, 0);
        assertTrue(listViewBase.canDragSheet(eventHeader));

        // Coordinates at the boundary of the list view should return false.
        MotionEvent eventListBoundary =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 500, listTop, 0);
        assertFalse(listViewBase.canDragSheet(eventListBoundary));

        // Coordinates inside the scrollable list view should return false.
        MotionEvent eventInList =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 500, listTop + 50, 0);
        assertFalse(listViewBase.canDragSheet(eventInList));

        // When list view is hidden, canDragSheet should return false.
        recyclerView.setVisibility(View.GONE);
        assertFalse(listViewBase.canDragSheet(eventHeader));

        // When content view is hidden, canDragSheet should return false.
        recyclerView.setVisibility(View.VISIBLE);
        contentView.setVisibility(View.GONE);
        assertFalse(listViewBase.canDragSheet(eventHeader));
    }

    private static View createMeasurableContentView(Context context) {
        View contentView =
                new View(context) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(800, 1000);
                    }
                };
        contentView.measure(
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        contentView.layout(0, 0, 800, 1000);
        return contentView;
    }

    private static View createMeasurableChildView(
            Context context, @Px int height, @Px int topMargin, @Px int bottomMargin) {
        View view =
                new View(context) {
                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(800, height);
                    }
                };
        view.measure(
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, 800, height);
        RecyclerView.LayoutParams params =
                new RecyclerView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, height);
        params.topMargin = topMargin;
        params.bottomMargin = bottomMargin;
        view.setLayoutParams(params);
        return view;
    }

    private static RecyclerView createMeasurableSheetRecyclerView(
            Context context, List<View> childViews) {
        return createMeasurableSheetRecyclerView(context, childViews, new int[] {0});
    }

    /**
     * Creates a list that shows exactly {@code childViews}. {@code scrollOffset[0]} is reported as
     * the current scroll position, so a test can change it to simulate scrolling.
     */
    private static RecyclerView createMeasurableSheetRecyclerView(
            Context context, List<View> childViews, int[] scrollOffset) {
        RecyclerView recyclerView =
                new RecyclerView(context) {
                    private @Nullable Adapter mAdapter;

                    @Override
                    public void setAdapter(@Nullable Adapter adapter) {
                        mAdapter = adapter;
                    }

                    @Override
                    public @Nullable Adapter getAdapter() {
                        return mAdapter;
                    }

                    @Override
                    public int getChildCount() {
                        return childViews.size();
                    }

                    @Override
                    public View getChildAt(int index) {
                        return childViews.get(index);
                    }

                    @Override
                    public int getChildAdapterPosition(View child) {
                        return childViews.indexOf(child);
                    }

                    @Override
                    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                        setMeasuredDimension(800, 1000);
                    }

                    @Override
                    public int computeVerticalScrollOffset() {
                        return scrollOffset[0];
                    }
                };
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 800, 1000);
        return recyclerView;
    }

    private static RecyclerView.Adapter createSheetAdapter(List<Integer> itemTypes) {
        return new RecyclerView.Adapter() {
            @Override
            public RecyclerView.ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
                return new RecyclerView.ViewHolder(new View(parent.getContext())) {};
            }

            @Override
            public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

            @Override
            public int getItemCount() {
                return itemTypes.size();
            }

            @Override
            public int getItemViewType(int position) {
                return itemTypes.get(position);
            }
        };
    }

    @Test
    public void testDesiredHeight_WhenMoreThanThreeItems_FourthItemPeeks() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = createMeasurableContentView(activity);
        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        final int handlebarHeight = 23;
        final int handlebarTopMargin = 3;
        final int handlebarBottomMargin = 5;
        final int headerHeight = 41;
        final int headerTopMargin = 2;
        final int headerBottomMargin = 11;
        // An odd height, so that half an item is not a whole number of pixels.
        final int itemHeight = 101;
        final int itemTopMargin = 7;
        final int itemBottomMargin = 13;
        final int fullyVisibleItemCount = BottomSheetListViewBase.MAX_FULLY_VISIBLE_LIST_ITEM_COUNT;
        listView.setHandlebarForTesting(
                createMeasurableChildView(
                        activity, handlebarHeight, handlebarTopMargin, handlebarBottomMargin));
        listView.setHeaderViewForTesting(
                createMeasurableChildView(
                        activity, headerHeight, headerTopMargin, headerBottomMargin));
        listView.setListedItemTypesForTesting(Set.of(0));

        // One more item than fits fully, so the last one peeks.
        List<View> childViews = new ArrayList<>();
        for (int i = 0; i < fullyVisibleItemCount + 1; i++) {
            childViews.add(
                    createMeasurableChildView(
                            activity, itemHeight, itemTopMargin, itemBottomMargin));
        }
        RecyclerView recyclerView = createMeasurableSheetRecyclerView(activity, childViews);
        RecyclerView.Adapter adapter =
                createSheetAdapter(Collections.nCopies(childViews.size(), 0));
        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        // The half-height sheet shows the handlebar, the header, the fully visible items, and the
        // top half of the peeking item (rounded down, with its top margin but not its bottom one).
        int expectedHalfHeight =
                (handlebarTopMargin + handlebarHeight + handlebarBottomMargin)
                        + (headerTopMargin + headerHeight + headerBottomMargin)
                        + fullyVisibleItemCount * (itemTopMargin + itemHeight + itemBottomMargin)
                        + (itemTopMargin + itemHeight / 2);
        assertEquals(
                "Half height should show the fully visible items plus half of the next item",
                expectedHalfHeight,
                listView.getDesiredSheetHeightPx());
    }

    @Test
    public void testDesiredHeight_WhenMoreThanFourItems_IgnoresFifthItemAndBeyond() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = createMeasurableContentView(activity);
        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        final int handlebarHeight = 23;
        final int handlebarTopMargin = 3;
        final int handlebarBottomMargin = 5;
        final int headerHeight = 41;
        final int headerTopMargin = 2;
        final int headerBottomMargin = 11;
        final int itemHeight = 101;
        final int itemTopMargin = 7;
        final int itemBottomMargin = 13;
        // Items after the peeking one are much taller, so any of them leaking into the half
        // height would be obvious.
        final int laterItemHeight = 997;
        final int laterItemTopMargin = 17;
        final int laterItemBottomMargin = 19;
        final int laterItemCount = 2;
        final int fullyVisibleItemCount = BottomSheetListViewBase.MAX_FULLY_VISIBLE_LIST_ITEM_COUNT;
        listView.setHandlebarForTesting(
                createMeasurableChildView(
                        activity, handlebarHeight, handlebarTopMargin, handlebarBottomMargin));
        listView.setHeaderViewForTesting(
                createMeasurableChildView(
                        activity, headerHeight, headerTopMargin, headerBottomMargin));
        listView.setListedItemTypesForTesting(Set.of(0));

        List<View> childViews = new ArrayList<>();
        // The fully visible items plus the peeking item.
        for (int i = 0; i < fullyVisibleItemCount + 1; i++) {
            childViews.add(
                    createMeasurableChildView(
                            activity, itemHeight, itemTopMargin, itemBottomMargin));
        }
        for (int i = 0; i < laterItemCount; i++) {
            childViews.add(
                    createMeasurableChildView(
                            activity, laterItemHeight, laterItemTopMargin, laterItemBottomMargin));
        }
        RecyclerView recyclerView = createMeasurableSheetRecyclerView(activity, childViews);
        RecyclerView.Adapter adapter =
                createSheetAdapter(Collections.nCopies(childViews.size(), 0));
        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        final int handlebarAndHeader =
                (handlebarTopMargin + handlebarHeight + handlebarBottomMargin)
                        + (headerTopMargin + headerHeight + headerBottomMargin);
        final int itemWithMargins = itemTopMargin + itemHeight + itemBottomMargin;

        // The half height stops at the peeking item; later items are not counted.
        int expectedHalfHeight =
                handlebarAndHeader
                        + fullyVisibleItemCount * itemWithMargins
                        + (itemTopMargin + itemHeight / 2);
        assertEquals(
                "Half height should ignore items after the peeking item",
                expectedHalfHeight,
                listView.getDesiredSheetHeightPx());

        // The full height shows every item with both margins.
        int expectedFullHeight =
                handlebarAndHeader
                        + (fullyVisibleItemCount + 1) * itemWithMargins
                        + laterItemCount
                                * (laterItemTopMargin + laterItemHeight + laterItemBottomMargin);
        assertEquals(
                "Full height should include every item and its margins",
                expectedFullHeight,
                listView.getMaximumSheetHeightPx());
    }

    @Test
    public void testHeight_FooterHiddenInHalfState_ShownInFullState() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = createMeasurableContentView(activity);
        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        final int listedType = 0;
        final int footerType = 1;
        final int handlebarHeight = 23;
        final int handlebarTopMargin = 3;
        final int handlebarBottomMargin = 5;
        final int headerHeight = 41;
        final int headerTopMargin = 2;
        final int headerBottomMargin = 11;
        final int itemHeight = 101;
        final int itemTopMargin = 7;
        final int itemBottomMargin = 13;
        final int footerHeight = 201;
        final int footerTopMargin = 4;
        final int footerBottomMargin = 17;
        final int conclusiveMargin = 29;
        // Fewer listed items than fit fully, so none of them peeks.
        final int listedItemCount = BottomSheetListViewBase.MAX_FULLY_VISIBLE_LIST_ITEM_COUNT - 1;
        listView.setHandlebarForTesting(
                createMeasurableChildView(
                        activity, handlebarHeight, handlebarTopMargin, handlebarBottomMargin));
        listView.setHeaderViewForTesting(
                createMeasurableChildView(
                        activity, headerHeight, headerTopMargin, headerBottomMargin));
        listView.setListedItemTypesForTesting(Set.of(listedType));
        listView.setFooterItemTypesForTesting(Set.of(footerType));
        listView.setConclusiveMarginHeightPxForTesting(conclusiveMargin);

        List<View> childViews = new ArrayList<>();
        List<Integer> itemTypes = new ArrayList<>();
        for (int i = 0; i < listedItemCount; i++) {
            childViews.add(
                    createMeasurableChildView(
                            activity, itemHeight, itemTopMargin, itemBottomMargin));
            itemTypes.add(listedType);
        }
        childViews.add(
                createMeasurableChildView(
                        activity, footerHeight, footerTopMargin, footerBottomMargin));
        itemTypes.add(footerType);

        RecyclerView recyclerView = createMeasurableSheetRecyclerView(activity, childViews);
        RecyclerView.Adapter adapter = createSheetAdapter(itemTypes);
        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        final int handlebarAndHeader =
                (handlebarTopMargin + handlebarHeight + handlebarBottomMargin)
                        + (headerTopMargin + headerHeight + headerBottomMargin);
        final int listedItems = listedItemCount * (itemTopMargin + itemHeight + itemBottomMargin);

        // Half height: the footer is hidden and the closing margin is added below the items.
        assertEquals(
                "Half height should hide the footer and add the closing margin",
                handlebarAndHeader + listedItems + conclusiveMargin,
                listView.getDesiredSheetHeightPx());

        // Full height: the footer is shown and the closing margin is left out.
        assertEquals(
                "Full height should include the listed items and the footer",
                handlebarAndHeader
                        + listedItems
                        + (footerTopMargin + footerHeight + footerBottomMargin),
                listView.getMaximumSheetHeightPx());
    }

    @Test
    public void testAdapterChanges_WhileScrolled_SheetReportsNewHeights() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = createMeasurableContentView(activity);
        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        final int handlebarHeight = 23;
        final int handlebarTopMargin = 3;
        final int handlebarBottomMargin = 5;
        final int itemTopMargin = 7;
        final int itemBottomMargin = 13;
        final int firstItemHeight = 101;
        final int itemHeightStep = 31;
        final int scrollDistance = 57;
        listView.setHandlebarForTesting(
                createMeasurableChildView(
                        activity, handlebarHeight, handlebarTopMargin, handlebarBottomMargin));

        List<View> childViews = new ArrayList<>();
        childViews.add(
                createMeasurableChildView(
                        activity, firstItemHeight, itemTopMargin, itemBottomMargin));
        int[] scrollOffset = {0};
        RecyclerView recyclerView =
                createMeasurableSheetRecyclerView(activity, childViews, scrollOffset);
        RecyclerView.Adapter adapter = createSheetAdapter(List.of(0));
        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        // Measure at the top of the list, then scroll down. While scrolled, the sheet keeps
        // reporting what it measured at the top until the adapter reports a change.
        listView.getDesiredSheetHeightPx();
        listView.getMaximumSheetHeightPx();
        scrollOffset[0] = scrollDistance;
        listView.getScrollListenerForTesting().onScrolled(recyclerView, 0, scrollDistance);

        Map<String, Runnable> updates = new LinkedHashMap<>();
        updates.put("notifyDataSetChanged()", adapter::notifyDataSetChanged);
        updates.put("notifyItemRangeInserted()", () -> adapter.notifyItemRangeInserted(0, 1));
        updates.put("notifyItemRangeRemoved()", () -> adapter.notifyItemRangeRemoved(0, 1));
        updates.put("notifyItemMoved()", () -> adapter.notifyItemMoved(0, 1));
        updates.put("notifyItemRangeChanged()", () -> adapter.notifyItemRangeChanged(0, 1));

        int itemHeight = firstItemHeight;
        for (Map.Entry<String, Runnable> update : updates.entrySet()) {
            String updateName = update.getKey();
            int reportedDesiredHeight = listView.getDesiredSheetHeightPx();
            int reportedMaximumHeight = listView.getMaximumSheetHeightPx();

            // Make the item taller without telling the sheet yet.
            itemHeight += itemHeightStep;
            childViews.set(
                    0,
                    createMeasurableChildView(
                            activity, itemHeight, itemTopMargin, itemBottomMargin));
            assertEquals(
                    "Before " + updateName + ", the scrolled sheet should keep its earlier height",
                    reportedDesiredHeight,
                    listView.getDesiredSheetHeightPx());
            assertEquals(
                    "Before " + updateName + ", the scrolled sheet should keep its earlier height",
                    reportedMaximumHeight,
                    listView.getMaximumSheetHeightPx());

            update.getValue().run();

            // The sheet holds the handlebar and the single item, each with both margins.
            int expectedHeight =
                    (handlebarTopMargin + handlebarHeight + handlebarBottomMargin)
                            + (itemTopMargin + itemHeight + itemBottomMargin);
            assertEquals(
                    "After " + updateName + ", the sheet should report the new half height",
                    expectedHeight,
                    listView.getDesiredSheetHeightPx());
            assertEquals(
                    "After " + updateName + ", the sheet should report the new full height",
                    expectedHeight,
                    listView.getMaximumSheetHeightPx());
        }
    }

    @Test
    public void testSheetClosed_NotifiesDismissHandlerAndRemovesObserver() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = new View(activity);
        TestBottomSheetListView listView =
                new TestBottomSheetListView(mMockBottomSheetController, contentView);
        RecyclerView recyclerView = new RecyclerView(activity);
        listView.setSheetItemListView(recyclerView);

        listView.setDismissHandler(mMockDismissHandler);

        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(listView);
        when(mMockBottomSheetController.requestShowContent(listView, true)).thenReturn(true);

        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        listView.setVisible(true);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());

        BottomSheetObserver observer = observerCaptor.getValue();

        // When another sheet is active, onSheetClosed should ignore.
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(null);
        observer.onSheetClosed(StateChangeReason.SWIPE);
        verify(mMockDismissHandler, never()).onResult(any());
        verify(mMockBottomSheetController, never()).removeObserver(observer);

        // When this sheet is active, onSheetClosed invokes dismiss handler and removes observer.
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(listView);
        observer.onSheetClosed(StateChangeReason.SWIPE);
        verify(mMockDismissHandler).onResult(StateChangeReason.SWIPE);
        verify(mMockBottomSheetController).removeObserver(observer);
    }

    @Test
    public void testDestroy_RemovesAllObservers() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        View contentView = new View(activity);
        RealMeasuringBottomSheetListView listView =
                new RealMeasuringBottomSheetListView(mMockBottomSheetController, contentView);

        RecyclerView recyclerView = new RecyclerView(activity);
        RecyclerView.Adapter adapter =
                new RecyclerView.Adapter() {
                    @Override
                    public RecyclerView.ViewHolder onCreateViewHolder(
                            ViewGroup parent, int viewType) {
                        return new RecyclerView.ViewHolder(new View(activity)) {};
                    }

                    @Override
                    public void onBindViewHolder(RecyclerView.ViewHolder holder, int position) {}

                    @Override
                    public int getItemCount() {
                        return 5;
                    }
                };

        listView.setSheetItemListView(recyclerView);
        listView.setSheetItemListAdapter(adapter);

        when(mMockBottomSheetController.requestShowContent(listView, true)).thenReturn(true);
        listView.setVisible(true);
        verify(mMockBottomSheetController).addObserver(listView.getBottomSheetObserverForTesting());

        // Destroy cleans up observers.
        listView.destroy();
        verify(mMockBottomSheetController)
                .removeObserver(listView.getBottomSheetObserverForTesting());

        // Detach the RecyclerView's own listener; after that, only the sheet could still be
        // listening to the adapter.
        recyclerView.setAdapter(null);
        assertFalse(
                "The destroyed sheet should no longer listen to adapter changes",
                adapter.hasObservers());
    }

    private static class TestItemDivider extends ItemDividerBase {
        private final Set<Integer> mSkippedItemTypes;

        TestItemDivider(Context context, Set<Integer> skippedItemTypes) {
            super(context);
            mSkippedItemTypes = skippedItemTypes;
        }

        @Override
        protected boolean shouldSkipItemType(int type) {
            return mSkippedItemTypes.contains(type);
        }
    }

    private static RecyclerView createItemDividerRecyclerView(
            Context context, RecyclerView.Adapter adapter, List<View> childViews) {
        return new RecyclerView(context) {
            @Override
            public @Nullable Adapter getAdapter() {
                return adapter;
            }

            @Override
            public @Nullable View getChildAt(int index) {
                // Like the real RecyclerView, return null for positions that have no child view.
                return index >= 0 && index < childViews.size() ? childViews.get(index) : null;
            }
        };
    }

    private static void assertItemHasBackgroundDrawable(Context context, View view) {
        assertNotNull(
                "Decorated view should have a background drawable applied", view.getBackground());
        assertEquals(
                "Decorated view should have correct background tint",
                ColorStateList.valueOf(SemanticColorUtils.getColorSurfaceContainerLow(context)),
                view.getBackgroundTintList());
    }

    /**
     * Corner radii of an item background, in the order top-left, top-right, bottom-right,
     * bottom-left.
     */
    private static float[] getItemCornerRadii(View view) {
        RippleDrawable ripple = (RippleDrawable) view.getBackground();
        GradientDrawable shape = (GradientDrawable) ripple.getDrawable(0);
        float[] radii = getPerCornerRadii(shape);
        if (radii == null) {
            // The shape uses one radius for every corner.
            float radius = shape.getCornerRadius();
            return new float[] {radius, radius, radius, radius};
        }
        // The radii come as (x, y) pairs per corner; the x value is enough here.
        return new float[] {radii[0], radii[2], radii[4], radii[6]};
    }

    /** Returns the per-corner radii of a shape, or null if the shape uses one radius. */
    private static @Nullable float[] getPerCornerRadii(GradientDrawable shape) {
        try {
            return shape.getCornerRadii();
        } catch (NullPointerException e) {
            // Some Android versions throw here instead of returning null when the shape uses one
            // radius for every corner.
            return null;
        }
    }

    private static void assertTopCornersRounderThanBottom(String message, View view) {
        float[] corners = getItemCornerRadii(view);
        assertTrue(message + " (left side)", corners[0] > corners[3]);
        assertTrue(message + " (right side)", corners[1] > corners[2]);
    }

    private static void assertBottomCornersRounderThanTop(String message, View view) {
        float[] corners = getItemCornerRadii(view);
        assertTrue(message + " (left side)", corners[3] > corners[0]);
        assertTrue(message + " (right side)", corners[2] > corners[1]);
    }

    @Test
    public void testItemDivider_SingleItem_RoundsAllCorners() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        View view0 = new View(activity);
        RecyclerView.Adapter adapter = createSheetAdapter(List.of(0));
        RecyclerView recyclerView =
                createItemDividerRecyclerView(activity, adapter, List.of(view0));

        TestItemDivider divider = new TestItemDivider(activity, Collections.emptySet());
        divider.onDraw(new Canvas(), recyclerView, new RecyclerView.State());

        assertItemHasBackgroundDrawable(activity, view0);
        float[] corners = getItemCornerRadii(view0);
        assertTrue("A single item should have rounded corners", corners[0] > 0);
        for (float corner : corners) {
            assertEquals(
                    "A single item should have all four corners equally round",
                    corners[0],
                    corner,
                    0.001f);
        }
    }

    @Test
    public void testItemDivider_TwoItems_RoundsTopAndBottomSeparately() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        View view0 = new View(activity);
        View view1 = new View(activity);
        RecyclerView.Adapter adapter = createSheetAdapter(List.of(0, 0));
        RecyclerView recyclerView =
                createItemDividerRecyclerView(activity, adapter, List.of(view0, view1));

        TestItemDivider divider = new TestItemDivider(activity, Collections.emptySet());
        divider.onDraw(new Canvas(), recyclerView, new RecyclerView.State());

        assertItemHasBackgroundDrawable(activity, view0);
        assertItemHasBackgroundDrawable(activity, view1);
        assertTopCornersRounderThanBottom(
                "The first item should be rounder at the top than at the bottom", view0);
        assertBottomCornersRounderThanTop(
                "The last item should be rounder at the bottom than at the top", view1);
    }

    @Test
    public void testItemDivider_MultipleItems_RoundsTopMiddleAndBottom() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        View view0 = new View(activity);
        View view1 = new View(activity);
        View view2 = new View(activity);
        View view3 = new View(activity);
        RecyclerView.Adapter adapter = createSheetAdapter(List.of(0, 0, 0, 0));
        RecyclerView recyclerView =
                createItemDividerRecyclerView(
                        activity, adapter, List.of(view0, view1, view2, view3));

        TestItemDivider divider = new TestItemDivider(activity, Collections.emptySet());
        divider.onDraw(new Canvas(), recyclerView, new RecyclerView.State());

        assertItemHasBackgroundDrawable(activity, view0);
        assertItemHasBackgroundDrawable(activity, view1);
        assertItemHasBackgroundDrawable(activity, view2);
        assertItemHasBackgroundDrawable(activity, view3);
        assertTopCornersRounderThanBottom(
                "The first item should be rounder at the top than at the bottom", view0);
        assertBottomCornersRounderThanTop(
                "The last item should be rounder at the bottom than at the top", view3);

        float firstItemTopCorner = getItemCornerRadii(view0)[0];
        for (View middleItem : List.of(view1, view2)) {
            float[] corners = getItemCornerRadii(middleItem);
            assertEquals(
                    "A middle item should be as round at the top as at the bottom",
                    corners[0],
                    corners[3],
                    0.001f);
            assertTrue(
                    "A middle item should be less round than the top of the first item",
                    corners[0] < firstItemTopCorner);
        }
    }

    @Test
    public void testItemDivider_SkipsHeadersAndFooters() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        final int headerType = 1;
        final int bodyType = 0;
        final int footerType = 2;
        List<View> views = new ArrayList<>();
        for (int i = 0; i < 6; i++) {
            views.add(new View(activity));
        }
        // Two headers, two body items, then two footers.
        RecyclerView.Adapter adapter =
                createSheetAdapter(
                        List.of(
                                headerType,
                                headerType,
                                bodyType,
                                bodyType,
                                footerType,
                                footerType));
        RecyclerView recyclerView = createItemDividerRecyclerView(activity, adapter, views);

        TestItemDivider divider = new TestItemDivider(activity, Set.of(headerType, footerType));
        divider.onDraw(new Canvas(), recyclerView, new RecyclerView.State());

        assertNull("The first header should remain undecorated", views.get(0).getBackground());
        assertNull("The second header should remain undecorated", views.get(1).getBackground());
        assertItemHasBackgroundDrawable(activity, views.get(2));
        assertItemHasBackgroundDrawable(activity, views.get(3));
        assertTopCornersRounderThanBottom(
                "The first body item should be rounder at the top than at the bottom",
                views.get(2));
        assertBottomCornersRounderThanTop(
                "The last body item should be rounder at the bottom than at the top", views.get(3));
        assertNull("The first footer should remain undecorated", views.get(4).getBackground());
        assertNull("The second footer should remain undecorated", views.get(5).getBackground());
    }

    @Test
    public void testItemDivider_WhenAllItemsSkipped_DoesNotAddDecorations() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        View view0 = new View(activity);
        View view1 = new View(activity);
        View view2 = new View(activity);
        // All positions are skipped (types 1 and 2).
        RecyclerView.Adapter adapter = createSheetAdapter(List.of(1, 1, 2));
        RecyclerView recyclerView =
                createItemDividerRecyclerView(activity, adapter, List.of(view0, view1, view2));

        TestItemDivider divider = new TestItemDivider(activity, Set.of(1, 2));
        divider.onDraw(new Canvas(), recyclerView, new RecyclerView.State());

        assertNull("Skipped item should remain undecorated", view0.getBackground());
        assertNull("Skipped item should remain undecorated", view1.getBackground());
        assertNull("Skipped item should remain undecorated", view2.getBackground());
    }
}
