// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.base.KeyNavigationUtil.isTabNavigation;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.util.MotionEventUtils;

/** A widget for showing a list of omnibox suggestions. */
@NullMarked
public class OmniboxSuggestionsDropdown extends RecyclerView {
    /** Used to tag and cancel the Accessibility focus events. */
    private static final Object TOKEN_ACCESSIBILITY_FOCUS = new Object();

    /**
     * Used to defer the accessibility announcement for list content. This makes core difference
     * when the list is first shown up, when the interaction with the Omnibox and presence of
     * virtual keyboard may actually cause throttling of the Accessibility events.
     *
     * <p>Note that this delay aims to strike a compromise between multiple directly competing
     * components for a11y time:
     *
     * <ul>
     *   <li>UrlBar: "facebook.com",
     *   <li>Soft Keyboard: "f. foxtrot. showing us english q w e r t y", and
     *   <li>Omnibox Suggestions: "15 suggested items in list below".
     * </ul>
     *
     * The Suggestions list can be announced after a slight pause, as it's best that it's announced
     * last.
     */
    private static final long LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS = 1500;

    private final SuggestionLayoutScrollListener mLayoutScrollListener;
    private final RecyclerViewSelectionController mSelectionController;
    private final Handler mHandler;

    private @Nullable OmniboxSuggestionsDropdownAdapter mAdapter;
    private @Nullable GestureObserver mGestureObserver;
    private float mChildVerticalTranslation;
    private float mChildAlpha = 1.0f;
    private boolean mToolbarOnTop = true;

    private final int mBaseBottomPadding;

    /**
     * Interface that will receive notifications when the user is interacting with an item on the
     * Suggestions list.
     */
    public interface GestureObserver {
        /**
         * Notify that the user is interacting with an item on the Suggestions list.
         *
         * @param isGestureUp Whether user pressed (false) or depressed (true) the element on the
         *     list.
         * @param timestamp The timestamp associated with the event.
         */
        void onGesture(boolean isGestureUp, long timestamp);
    }

    /** Scroll manager that propagates scroll event notification to registered observers. */
    @VisibleForTesting
    /* package */ static class SuggestionLayoutScrollListener extends LinearLayoutManager {
        private boolean mIsScrolledToTop;
        private boolean mCurrentGestureAffectedKeyboardState;
        private @Nullable Runnable mSuggestionDropdownScrollListener;
        private @Nullable Runnable mSuggestionDropdownOverscrolledToTopListener;
        private boolean mToolbarOnTop = true;

        public SuggestionLayoutScrollListener(Context context) {
            super(context);
            mIsScrolledToTop = true;
        }

        void setToolbarPosition(boolean isToolbarOnTop) {
            mToolbarOnTop = isToolbarOnTop;
            setStackFromEnd(!isToolbarOnTop);
            setReverseLayout(!isToolbarOnTop);
        }

        @Override
        public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) {
            scrollToPositionWithOffset(0, 0);
            super.onLayoutChildren(recycler, state);
        }

        @Override
        public int scrollVerticallyBy(
                int requestedDeltaY, RecyclerView.Recycler recycler, RecyclerView.State state) {
            int resultingDeltaY = super.scrollVerticallyBy(requestedDeltaY, recycler, state);
            return updateKeyboardVisibilityAndScroll(resultingDeltaY, requestedDeltaY);
        }

        /**
         * Respond to scroll event.
         *
         * <ul>
         *   <li>Upon scroll down from the top, if the distance scrolled is same as distance
         *       requested (= the list has enough content to respond to the request), hide the
         *       keyboard and suppress the scroll action by reporting 0 as the resulting scroll
         *       distance.
         *   <li>Upon scroll up to the top, if the distance scrolled is shorter than the distance
         *       requested (= the list has reached the top), show the keyboard.
         *   <li>In all other cases, take no action.
         * </ul>
         *
         * <p>The code reports 0 if and only if the keyboard state transitions from "shown" to
         * "hidden".
         *
         * <p>The logic remembers the last requested keyboard state, so that the keyboard is not
         * repeatedly called up or requested to be hidden.
         *
         * @param resultingDeltaY The scroll distance by which the LayoutManager intends to scroll.
         *     Negative values indicate scroll up, positive values indicate scroll down.
         * @param requestedDeltaY The scroll distance requested by the user via gesture. Negative
         *     values indicate scroll up, positive values indicate scroll down.
         * @return Value of resultingDeltaY, if scroll is permitted, or 0 when it is suppressed.
         */
        @VisibleForTesting
        /* package */ int updateKeyboardVisibilityAndScroll(
                int resultingDeltaY, int requestedDeltaY) {
            // Change keyboard visibility only once per gesture.
            // This helps in situations where the user interacts with the horizontal caoursel (e.g.
            // the Most Visited Sites), where a horizontal finger swipe could result in a series of
            // keyboard show/hide events.
            if (mCurrentGestureAffectedKeyboardState) return resultingDeltaY;

            // If the effective scroll distance is:
            // - same as the desired one, we have enough content to scroll in a given direction
            //   (negative values = up, positive values = down).
            // - if resultingDeltaY is smaller than requestedDeltaY, we have reached the bottom of
            //   the list. This can occur only if both values are greater than or equal to 0:
            //   having reached the bottom of the list, the scroll request cannot be satisfied and
            //   the resultingDeltaY is clamped.
            // - if resultingDeltaY is greater than requestedDeltaY, we have reached the top of the
            //   list. This can occur only if both values are less than or equal to zero:
            //   having reached the top of the list, the scroll request cannot be satisfied and
            //   the resultingDeltaY is clamped.
            //
            // When resultingDeltaY is less than requestedDeltaY we know we have reached the bottom
            // of the list and weren't able to satisfy the requested scroll distance.
            // This could happen in one of two cases:
            // 1. the list was previously scrolled down (and we have already toggled keyboard
            //    visibility), or
            // 2. the list is too short, and almost entirely fits on the screen, leaving at most
            //    just a few pixels of content hiding under the keyboard.
            // Note that the list may extend below the keyboard and still be non-scrollable:
            // http://crbug/1479437

            // Otherwise decide whether keyboard should be shown or not.
            // We want to call keyboard up only when we know we reached the top of the list.
            // Note: the condition below evaluates `true` only if the scroll direction is "up",
            // meaning values are <= 0, meaning all three conditions are true:
            // - resultingDeltaY <= 0
            // - requestedDeltaY <= 0
            // - Math.abs(resultingDeltaY) < Math.abs(requestedDeltaY)
            boolean newIsScrolledToTop =
                    mToolbarOnTop
                            ? (resultingDeltaY > requestedDeltaY)
                            : (resultingDeltaY < requestedDeltaY);

            if (mIsScrolledToTop == newIsScrolledToTop) return resultingDeltaY;
            mIsScrolledToTop = newIsScrolledToTop;
            mCurrentGestureAffectedKeyboardState = true;

            if (mIsScrolledToTop) {
                if (mSuggestionDropdownOverscrolledToTopListener != null) {
                    mSuggestionDropdownOverscrolledToTopListener.run();
                }
            } else {
                if (mSuggestionDropdownScrollListener != null) {
                    mSuggestionDropdownScrollListener.run();
                }
                return 0;
            }
            return resultingDeltaY;
        }

        @Override
        public LayoutParams generateDefaultLayoutParams() {
            RecyclerView.LayoutParams params = super.generateDefaultLayoutParams();
            params.width = RecyclerView.LayoutParams.MATCH_PARENT;
            return params;
        }

        /**
         * Reset the internal scroll tracker. This needs to be called either when the
         * SuggestionsDropdown is hidden or shown again to reflect either the end of the current or
         * beginning of the next interaction session.
         */
        @VisibleForTesting
        /* package */ void resetScrollState() {
            mIsScrolledToTop = true;
            mCurrentGestureAffectedKeyboardState = false;
            updateVisualScrollState();
        }

        /* package */ void updateVisualScrollState() {
            if (!mIsScrolledToTop) return;
            postOnAnimation(() -> scrollToPositionWithOffset(0, 0));
        }

        /**
         * Reset internal state, preparing to handle a new gesture. Note: currently invoked both
         * when a gesture begins and ends.
         */
        /* package */ void onNewGesture() {
            mCurrentGestureAffectedKeyboardState = false;
        }

        /**
         * @param listener A listener will be invoked whenever the User scrolls the list.
         */
        public void setSuggestionDropdownScrollListener(Runnable listener) {
            mSuggestionDropdownScrollListener = listener;
        }

        /**
         * @param listener A listener will be invoked whenever the User scrolls the list to the top.
         */
        public void setSuggestionDropdownOverscrolledToTopListener(Runnable listener) {
            mSuggestionDropdownOverscrolledToTopListener = listener;
        }
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     *
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsDropdown(Context context, AttributeSet attrs) {
        this(context, attrs, new SuggestionLayoutScrollListener(context));
    }

    @VisibleForTesting
    OmniboxSuggestionsDropdown(
            Context context,
            AttributeSet attrs,
            SuggestionLayoutScrollListener suggestionLayoutScrollListener) {
        super(context, attrs, android.R.attr.dropDownListViewStyle);

        mHandler = new Handler(Looper.getMainLooper());

        setFocusable(true);
        setFocusableInTouchMode(true);
        setId(R.id.omnibox_suggestions_dropdown);

        // By default RecyclerViews come with item animators.
        setItemAnimator(null);
        addItemDecoration(new SuggestionHorizontalDivider(context));

        mLayoutScrollListener = suggestionLayoutScrollListener;
        setLayoutManager(mLayoutScrollListener);
        mSelectionController =
                new RecyclerViewSelectionController(
                        mLayoutScrollListener, SelectionController.Mode.SATURATING_WITH_SENTINEL);
        addOnChildAttachStateChangeListener(mSelectionController);

        final Resources resources = context.getResources();
        mBaseBottomPadding =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        int paddingTop =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_top);
        this.setPaddingRelative(0, paddingTop, 0, mBaseBottomPadding);

        // Disable the scrollbar since it causes the hover events happening near the
        // scrollbar not dispatched to the underlying views.
        setVerticalScrollBarEnabled(false);

        if (OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            setRecycledViewPool(new PreWarmingRecycledViewPool(mAdapter, context));
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldW, int oldH) {
        mLayoutScrollListener.updateVisualScrollState();
    }

    /**
     * Set whether the dropdown should be clipped to its outline.
     *
     * @param clip whether to clip the outline
     */
    public void setShouldClipToOutline(boolean clip) {
        if (clip) {
            setOutlineProvider(
                    new RoundedCornerOutlineProvider(
                            getContext()
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen
                                                    .omnibox_suggestion_dropdown_round_corner_radius)));
            setClipToOutline(true);
        } else {
            setOutlineProvider(null);
            setClipToOutline(false);
        }
    }

    /** Get the Android View implementing suggestion list. */
    public ViewGroup getViewGroup() {
        return this;
    }

    /** Clean up resources and remove observers installed by this class. */
    public void destroy() {
        getRecycledViewPool().clear();
        mGestureObserver = null;
    }

    /**
     * Sets the observer for that the user is interacting with an item on the Suggestions list..
     *
     * @param observer an observer of this gesture.
     */
    public void setGestureObserver(OmniboxSuggestionsDropdown.GestureObserver observer) {
        mGestureObserver = observer;
    }

    /** Resets selection typically in response to changes to the list. */
    public void resetSelection() {
        mSelectionController.reset();
    }

    /**
     * Translates all children by {@code translation}. This translation is applied to newly-added
     * added children as well.
     */
    public void translateChildrenVertical(float translation) {
        mChildVerticalTranslation = translation;
        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            getChildAt(i).setTranslationY(translation);
        }
        invalidateItemDecorations();
    }

    /**
     * Sets the alpha of all child views. This alpha is applied to newly-added added children as
     * well.
     */
    public void setChildAlpha(float alpha) {
        mChildAlpha = alpha;
        final int childCount = getChildCount();
        for (int i = 0; i < childCount; i++) {
            getChildAt(i).setAlpha(alpha);
        }
        invalidateItemDecorations();
    }

    @Override
    public void onChildAttachedToWindow(View child) {
        child.setAlpha(mChildAlpha);
        if (mChildVerticalTranslation != 0.0f) {
            child.setTranslationY(mChildVerticalTranslation);
        }
    }

    @Override
    public void onChildDetachedFromWindow(View child) {
        child.setTranslationY(0.0f);
        child.setAlpha(1.0f);
    }

    /** Resests the tracked keyboard shown state to properly respond to scroll events. */
    void resetScrollState() {
        mLayoutScrollListener.resetScrollState();
    }

    /**
     * @return The number of items in the list.
     */
    public int getDropdownItemViewCountForTest() {
        if (mAdapter == null) return 0;
        return mAdapter.getItemCount();
    }

    /**
     * @return The Suggestion view at specific index.
     */
    public @Nullable View getDropdownItemViewForTest(int index) {
        final LayoutManager manager = assumeNonNull(getLayoutManager());
        manager.scrollToPosition(index);
        return manager.findViewByPosition(index);
    }

    @Override
    public void setAdapter(@Nullable Adapter adapter) {
        mAdapter = (OmniboxSuggestionsDropdownAdapter) adapter;
        super.setAdapter(mAdapter);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Layout");
                TimingMetric metric = OmniboxMetrics.recordSuggestionListLayoutTime();
                TimingMetric metric2 = OmniboxMetrics.recordSuggestionListLayoutWallTime()) {
            super.onLayout(changed, l, t, r, b);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        View selectedView = mSelectionController.getSelectedView();
        if (selectedView != null && selectedView.onKeyDown(keyCode, event)) {
            return true;
        }

        if (isTabNavigation(event)) {
            boolean maybeProcessed = super.onKeyDown(keyCode, event);
            if (maybeProcessed) return true;
            if (event.isShiftPressed()) {
                return mSelectionController.selectPreviousItem();
            }
            return mSelectionController.selectNextItem();
        }

        boolean isGoDownKey = KeyNavigationUtil.isGoDown(event);
        boolean isGoUpKey = KeyNavigationUtil.isGoUp(event);

        if ((mToolbarOnTop && isGoDownKey) || (!mToolbarOnTop && isGoUpKey)) {
            mSelectionController.selectNextItem();
            return true;
        } else if ((mToolbarOnTop && isGoUpKey) || (!mToolbarOnTop && isGoDownKey)) {
            mSelectionController.selectPreviousItem();
            return true;
        } else if (KeyNavigationUtil.isEnter(event)) {
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // For some reason, RecyclerView.onGenericMotionEvent() always returns false even after
        // handling events. Consume mouse/trackpad events to ensure clicks and scroll do not
        // bleed through to sibling views that are obscured by the list.  crbug.com/968414
        int action = event.getActionMasked();
        boolean shouldConsumeGenericMotionEvent =
                (MotionEventUtils.isPointerEvent(event)
                        && (action == MotionEvent.ACTION_BUTTON_PRESS
                                || action == MotionEvent.ACTION_BUTTON_RELEASE
                                || action == MotionEvent.ACTION_SCROLL));
        return super.onGenericMotionEvent(event) || shouldConsumeGenericMotionEvent;
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        final int eventType = ev.getActionMasked();
        if (eventType == MotionEvent.ACTION_UP || eventType == MotionEvent.ACTION_DOWN) {
            mLayoutScrollListener.onNewGesture();
            if (mGestureObserver != null) {
                mGestureObserver.onGesture(eventType == MotionEvent.ACTION_UP, ev.getEventTime());
            }
        }
        return super.dispatchTouchEvent(ev);
    }

    public void emitWindowContentChangedAnnouncement() {
        cancelWindowContentChangedAnnouncement();

        @StringRes
        int announcedStringRes =
                mToolbarOnTop
                        ? R.string.accessibility_omnibox_suggested_items
                        : R.string.accessibility_omnibox_suggested_items_above;

        // Note: can't use postDelayed until minSdk is 28.
        mHandler.postAtTime(
                () -> {
                    setAccessibilityLiveRegion(ACCESSIBILITY_LIVE_REGION_POLITE);
                    setContentDescription(
                            getContext()
                                    .getString(
                                            announcedStringRes,
                                            mAdapter == null ? 0 : mAdapter.getItemCount()));
                    sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
                    setAccessibilityLiveRegion(ACCESSIBILITY_LIVE_REGION_NONE);
                },
                TOKEN_ACCESSIBILITY_FOCUS,
                TimeUtils.uptimeMillis() + LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS);
    }

    /* package */ void cancelWindowContentChangedAnnouncement() {
        mHandler.removeCallbacksAndMessages(TOKEN_ACCESSIBILITY_FOCUS);
    }

    void setToolbarPosition(@ControlsPosition int toolbarPosition) {
        mToolbarOnTop =
                !(ChromeFeatureList.sAndroidBottomToolbarV2ReverseOrderSuggestionsList.getValue()
                        && toolbarPosition == ControlsPosition.BOTTOM);
        mLayoutScrollListener.setToolbarPosition(mToolbarOnTop);

        var params = (FrameLayout.LayoutParams) getLayoutParams();
        params.gravity = mToolbarOnTop ? Gravity.TOP : Gravity.BOTTOM;
        setLayoutParams(params);
    }

    /** Returns whether the toolbar is currently positioned on top. For testing purposes only. */
    boolean getToolbarOnTopForTesting() {
        return mToolbarOnTop;
    }

    @VisibleForTesting
    SuggestionLayoutScrollListener getLayoutScrollListener() {
        return mLayoutScrollListener;
    }

    @VisibleForTesting
    int getBaseBottomPadding() {
        return mBaseBottomPadding;
    }

    @VisibleForTesting
    @Override
    public @Nullable OmniboxSuggestionsDropdownAdapter getAdapter() {
        return mAdapter;
    }
}
