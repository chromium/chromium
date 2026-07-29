// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.RelativeLayout;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.UiAndroidFeatureList;
import org.chromium.ui.base.ViewUtils;

import java.util.Set;

/**
 * A generic base class for list-based bottom sheets.
 *
 * <p>This class manages the lifecycle, sizing, and scroll behavior of bottom sheets that primarily
 * display a list of items (using RecyclerView).
 *
 * <p>Sizing and Sizing Logic: The sheet supports two main height states: - HALF state (Desired
 * Height): Designed to show a limited set of items. It displays up to
 * MAX_FULLY_VISIBLE_LIST_ITEM_COUNT (typically 3) items fully. If there are more items, it displays
 * a partial item (peeking) to visually cue the user that more content is available by scrolling.
 * Footer items are excluded from this state. - FULL state (Maximum Height): Designed to show the
 * entire content, including any footer items. Sizing is calculated dynamically by measuring the
 * header, handlebar, and list items.
 *
 * <p>Scroll Behavior: - In the FULL state, the list is scrollable. - In the HALF state, scrolling
 * is initially disabled if the list is at the top. This allows drag gestures on the list to drag
 * the sheet up to the FULL state instead of scrolling the list. If the sheet is in the HALF state
 * but the list is already scrolled down (which can happen during transitions), it remains
 * scrollable until the user scrolls back to the top.
 *
 * <p>Footer Concepts: Subclasses can define footer items by returning their view types in
 * footerItemTypes(). Footer items are treated differently from regular list items: they are only
 * shown when the sheet is fully extended (FULL state) and are positioned at the bottom of the list.
 */
@NullMarked
public abstract class BottomSheetListViewBase implements BottomSheetContent {
    public static final int MAX_FULLY_VISIBLE_LIST_ITEM_COUNT = 3;

    private final BottomSheetController mBottomSheetController;
    private final RelativeLayout mContentView;
    private final BottomSheetRecyclerScrollListener mScrollListener;
    private final boolean mSuppressCollectionA11y;
    private @Nullable Callback<Integer> mDismissHandler;
    // Current scrollable surface on the screen that is updated whenever the user navigates between
    // the screens in the bottom sheets. For example, if the bottom sheet has multiple screens
    // (e.g. main and detail screens).
    private @Nullable RecyclerView mSheetItemListView;

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                    if (mBottomSheetController.getCurrentSheetContent()
                            != BottomSheetListViewBase.this) {
                        return;
                    }
                    super.onSheetClosed(reason);
                    assert mDismissHandler != null;
                    mDismissHandler.onResult(reason);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }

                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    if (mBottomSheetController.getCurrentSheetContent()
                            != BottomSheetListViewBase.this) {
                        return;
                    }
                    super.onSheetStateChanged(newState, reason);
                    if (newState == BottomSheetController.SheetState.FULL) {
                        // The list of items should be scrollable in full state.
                        assumeNonNull(mSheetItemListView).suppressLayout(false);
                    } else if (newState == BottomSheetController.SheetState.HALF
                            && mScrollListener.isScrolledToTop()) {
                        // The list of items should not be scrollable when the sheet transitions
                        // into half state if it's scrolled to the top. If the list is currently
                        // scrolled away from the top, it should stay scrolled in half state until
                        // the user scrolls to the top.
                        assumeNonNull(mSheetItemListView).suppressLayout(true);
                    }
                    if (newState != BottomSheetController.SheetState.HIDDEN) return;
                    // This is a fail-safe for cases where onSheetClosed isn't triggered.
                    assumeNonNull(mDismissHandler);
                    mDismissHandler.onResult(BottomSheetController.StateChangeReason.NONE);
                    mBottomSheetController.removeObserver(mBottomSheetObserver);
                }
            };

    /**
     * Used to access the handlebar to measure it.
     *
     * @return the {@link View} representing the drag handlebar.
     */
    protected abstract View getHandlebar();

    /**
     * Used to access the header view to measure it.
     *
     * @return the {@link View} representing the bottom sheet header view.
     */
    protected abstract @Nullable View getHeaderView();

    /**
     * Returns the margin between the last item in the scrollable list and the footer.
     *
     * @return the margin size in pixels.
     */
    protected abstract @Px int getConclusiveMarginHeightPx();

    /**
     * Used as a helper to measure the size of the sheet content.
     *
     * @return the side margin of the content view.
     */
    protected abstract @Px int getSideMarginPx();

    /**
     * Used as a helper for the list item height calculation.
     *
     * @return the item types of the list items on the {@link BottomSheet}.
     */
    protected abstract Set<Integer> listedItemTypes();

    /**
     * Used as a helper for the list item height calculation.
     *
     * @return the item types of the footer on the {@link BottomSheet}.
     */
    protected abstract Set<Integer> footerItemTypes();

    /**
     * Constructor for BottomSheetListViewBase. Controls the bottom sheet and its content view.
     *
     * <p>If suppressCollectionA11y is true, the screen reader will not automatically announce the
     * index of the item in the list. This is useful when the list contains non-item elements like
     * headers or footers that would distort the count. In this case, you must manually set the
     * content description on the item views during binding to ensure accessibility.
     *
     * @param bottomSheetController The BottomSheetController used to show/hide the sheet.
     * @param contentView The content of the bottom sheet.
     * @param suppressCollectionA11y Disables/enables setting the collection related a11y node info.
     */
    public BottomSheetListViewBase(
            BottomSheetController bottomSheetController,
            RelativeLayout contentView,
            Boolean suppressCollectionA11y) {
        mBottomSheetController = bottomSheetController;
        mContentView = contentView;
        mContentView.setOnGenericMotionListener((v, e) -> true); // Filter background interaction.

        mScrollListener = new BottomSheetRecyclerScrollListener(mBottomSheetController);
        mSuppressCollectionA11y = suppressCollectionA11y;
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    public void setSheetItemListAdapter(RecyclerView.Adapter adapter) {
        assumeNonNull(mSheetItemListView).setAdapter(assertNonNull(adapter));
    }

    public void setSheetItemListView(RecyclerView sheetItemListView) {
        mSheetItemListView = assertNonNull(sheetItemListView);

        mSheetItemListView.setLayoutManager(
                new LinearLayoutManager(
                        mSheetItemListView.getContext(), LinearLayoutManager.VERTICAL, false) {
                    @Override
                    public boolean isAutoMeasureEnabled() {
                        return true;
                    }

                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            RecyclerView.Recycler recycler,
                            RecyclerView.State state,
                            AccessibilityNodeInfoCompat info) {
                        if (!mSuppressCollectionA11y) {
                            super.onInitializeAccessibilityNodeInfo(recycler, state, info);
                        }
                    }
                });
        mSheetItemListView.addOnScrollListener(mScrollListener);
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean describing whether to show or hide the sheet.
     * @return True if the request was successful, false otherwise
     */
    public boolean setVisible(boolean isVisible) {
        if (isVisible) {
            remeasure();
            mBottomSheetController.addObserver(mBottomSheetObserver);
            if (!mBottomSheetController.requestShowContent(this, true)) {
                return false;
            }
        } else {
            mBottomSheetController.hideContent(this, true);
        }
        return true;
    }

    /**
     * Sets a new listener that reacts to events like item selection or dismissal.
     *
     * @param dismissHandler A {@link Callback<Integer>}.
     */
    public void setDismissHandler(Callback<Integer> dismissHandler) {
        mDismissHandler = dismissHandler;
    }

    /**
     * Returns the height of the full state. Must show the footer items permanently. For up to four
     * list items, the sheet usually cannot fill the screen.
     *
     * @return the full state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    protected @Px int getMaximumSheetHeightPx() {
        if (assumeNonNull(mSheetItemListView).getAdapter() == null) {
            // TODO(crbug.com/40843561): Assert this condition in setVisible. Should never happen.
            return BottomSheetContent.HeightMode.DEFAULT;
        }
        @Px int requiredMaxHeight = getHeightWhenFullyExtendedPx();
        if (UiAndroidFeatureList.sBottomSheetRemeasureFix.isEnabled()
                || requiredMaxHeight <= mBottomSheetController.getContainerHeight()) {
            return requiredMaxHeight;
        }
        remeasure();
        ViewUtils.requestLayout(mContentView, "BottomSheetListViewBase.getMaximumSheetHeightPx");
        return getHeightWhenFullyExtendedPx();
    }

    /**
     * Returns the height of the half state. Does not show the footer items. For 1 list item (plus
     * action button), 2 or 3 list items, it shows all items fully. For 4+ list items, it shows the
     * first 3.5 list items to encourage scrolling.
     *
     * @return the half state height in pixels. Never 0. Can theoretically exceed the screen height.
     */
    protected @Px int getDesiredSheetHeightPx() {
        if (assumeNonNull(mSheetItemListView).getAdapter() == null) {
            // TODO(crbug.com/40843561): Assert this condition in setVisible. Should never happen.
            return BottomSheetContent.HeightMode.DEFAULT;
        }
        int height =
                getHeightWithMarginsPx(getHandlebar(), false)
                        + getHeightWithMarginsPx(getHeaderView(), false)
                        + getSheetItemListHeightWithMarginsPx(true);
        return height;
    }

    private @Px int getHeightWhenFullyExtendedPx() {
        assert mContentView.getMeasuredHeight() > 0 : "ContentView hasn't been measured.";
        int height =
                getHeightWithMarginsPx(getHandlebar(), false)
                        + getHeightWithMarginsPx(getHeaderView(), false)
                        + getSheetItemListHeightWithMarginsPx(false);
        return height;
    }

    private @Px int getSheetItemListHeightWithMarginsPx(boolean showOnlyInitialItems) {
        assert assumeNonNull(mSheetItemListView).getMeasuredHeight() > 0
                : "Sheet item list hasn't been measured.";
        @Px int totalHeight = 0;
        int visibleItems = 0;
        for (int posInSheet = 0; posInSheet < mSheetItemListView.getChildCount(); posInSheet++) {
            View child = mSheetItemListView.getChildAt(posInSheet);
            if (isListedItem(child)) {
                // Counting how many clickable list items are displayed.
                visibleItems++;
            } else if (showOnlyInitialItems && isFooterItem(child)) {
                // If we want to show only the initial items, the footer should remain hidden.
                return totalHeight + getConclusiveMarginHeightPx();
            }
            if (showOnlyInitialItems && visibleItems > MAX_FULLY_VISIBLE_LIST_ITEM_COUNT) {
                // If the current item is the last to be shown, skip remaining elements and margins.
                totalHeight += getHeightWithMarginsPx(child, true);
                return totalHeight;
            }
            totalHeight += getHeightWithMarginsPx(child, false);
        }
        return totalHeight;
    }

    private static @Px int getHeightWithMarginsPx(@Nullable View view, boolean shouldPeek) {
        if (view == null) {
            return 0;
        }
        assert view.getMeasuredHeight() > 0 : "View hasn't been measured.";
        return getMarginsPx(view, /* excludeBottomMargin= */ shouldPeek)
                + (shouldPeek ? view.getMeasuredHeight() / 2 : view.getMeasuredHeight());
    }

    private static @Px int getMarginsPx(View view, boolean excludeBottomMargin) {
        LayoutParams params = view.getLayoutParams();
        if (params instanceof MarginLayoutParams) {
            MarginLayoutParams marginParams = (MarginLayoutParams) params;
            return marginParams.topMargin + (excludeBottomMargin ? 0 : marginParams.bottomMargin);
        }
        return 0;
    }

    /** Measures the content of the bottom sheet. */
    protected void remeasure() {
        mContentView.measure(
                View.MeasureSpec.makeMeasureSpec(getInsetDisplayWidthPx(), MeasureSpec.AT_MOST),
                MeasureSpec.UNSPECIFIED);
        assumeNonNull(mSheetItemListView)
                .measure(
                        View.MeasureSpec.makeMeasureSpec(
                                getInsetDisplayWidthPx(), MeasureSpec.AT_MOST),
                        MeasureSpec.UNSPECIFIED);
    }

    protected void removeObserver(BottomSheetObserver observer) {
        mBottomSheetController.removeObserver(observer);
    }

    protected boolean isFullyExtended() {
        return mBottomSheetController.getCurrentOffset()
                == Math.min(getMaximumSheetHeightPx(), mBottomSheetController.getContainerHeight());
    }

    private @Px int getInsetDisplayWidthPx() {
        return mContentView.getContext().getResources().getDisplayMetrics().widthPixels
                - 2 * getSideMarginPx();
    }

    private boolean isListedItem(View childInSheetView) {
        int posInAdapter =
                assumeNonNull(mSheetItemListView).getChildAdapterPosition(childInSheetView);
        assumeNonNull(mSheetItemListView.getAdapter());
        return listedItemTypes()
                .contains(mSheetItemListView.getAdapter().getItemViewType(posInAdapter));
    }

    private boolean isFooterItem(View childInSheetView) {
        int posInAdapter =
                assumeNonNull(mSheetItemListView).getChildAdapterPosition(childInSheetView);
        assumeNonNull(mSheetItemListView.getAdapter());
        return footerItemTypes()
                .contains(mSheetItemListView.getAdapter().getItemViewType(posInAdapter));
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        return false;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean skipHalfStateOnScrollingDown() {
        // Skip the half state if a service requesting touch exploration is enabled.
        return AccessibilityState.isTouchExplorationEnabled();
    }

    @Override
    public float getFullHeightRatio() {
        // WRAP_CONTENT would be the right fit but this disables the HALF state.
        return Math.min(getMaximumSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public float getHalfHeightRatio() {
        // Disable the half state when touch exploration is enabled.
        if (skipHalfStateOnScrollingDown()) return HeightMode.DISABLED;
        return Math.min(getDesiredSheetHeightPx(), mBottomSheetController.getContainerHeight())
                / (float) mBottomSheetController.getContainerHeight();
    }

    @Override
    public boolean hideOnScroll() {
        return false;
    }

    @Override
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    public void updateScreenHeight() {
        remeasure();
        // Use post() to ensure the RecyclerView has finished laying out the new items before the
        // sheet resizes.
        assumeNonNull(mSheetItemListView);
        mSheetItemListView.post(() -> mBottomSheetController.expandSheet());
    }

    /**
     * Applies RTL layout changes to the content view for testing purposes.
     *
     * <p>In production code, layout direction is set naturally by the system or parent view. In
     * render tests, this method can be called to explicitly set the layout direction based on
     * {@link LocalizationUtils#isLayoutRtl()}.
     */
    public void applyRtlLayoutForTesting() {
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;
        mContentView.setLayoutDirection(layoutDirection);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public RecyclerView getSheetItemListView() {
        return assertNonNull(mSheetItemListView);
    }
}
