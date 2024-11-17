// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

import java.util.Optional;

/** A widget for showing a list of omnibox suggestions. */
public class OmniboxSuggestionsDropdown extends RecyclerView {
    /**
     * Used to defer the accessibility announcement for list content. This makes core difference
     * when the list is first shown up, when the interaction with the Omnibox and presence of
     * virtual keyboard may actually cause throttling of the Accessibility events.
     */
    private static final long LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS = 300;

    private final SuggestionLayoutScrollListener mLayoutScrollListener;
    private final RecyclerViewSelectionController mSelectionController;

    private @Nullable OmniboxSuggestionsDropdownAdapter mAdapter;
    private Optional<OmniboxSuggestionsDropdownEmbedder> mEmbedder = Optional.empty();
    private @Nullable GestureObserver mGestureObserver;
    private @Nullable Callback<Integer> mHeightChangeListener;
    private @NonNull OmniboxAlignment mOmniboxAlignment = OmniboxAlignment.UNSPECIFIED;

    private int mListViewMaxHeight;
    private int mLastBroadcastedListViewMaxHeight;
    private @Nullable Callback<OmniboxAlignment> mOmniboxAlignmentObserver =
            this::onOmniboxAlignmentChanged;
    private float mChildVerticalTranslation;
    private float mChildAlpha = 1.0f;

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
        private boolean mLastKeyboardShownState;
        private boolean mCurrentGestureAffectedKeyboardState;
        private @Nullable Runnable mSuggestionDropdownScrollListener;
        private @Nullable Runnable mSuggestionDropdownOverscrolledToTopListener;

        public SuggestionLayoutScrollListener(Context context) {
            super(context);
            mLastKeyboardShownState = true;
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
            boolean keyboardShouldShow = (resultingDeltaY > requestedDeltaY);

            if (mLastKeyboardShownState == keyboardShouldShow) return resultingDeltaY;
            mLastKeyboardShownState = keyboardShouldShow;
            mCurrentGestureAffectedKeyboardState = true;

            if (keyboardShouldShow) {
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
         * Reset the internal keyboard state. This needs to be called either when the
         * SuggestionsDropdown is hidden or shown again to reflect either the end of the current or
         * beginning of the next interaction session.
         */
        @VisibleForTesting
        /* package */ void resetKeyboardShownState() {
            mLastKeyboardShownState = true;
            mCurrentGestureAffectedKeyboardState = false;
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
        public void setSuggestionDropdownScrollListener(@NonNull Runnable listener) {
            mSuggestionDropdownScrollListener = listener;
        }

        /**
         * @param listener A listener will be invoked whenever the User scrolls the list to the top.
         */
        public void setSuggestionDropdownOverscrolledToTopListener(@NonNull Runnable listener) {
            mSuggestionDropdownOverscrolledToTopListener = listener;
        }
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     *
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsDropdown(@NonNull Context context, AttributeSet attrs) {
        super(context, attrs, android.R.attr.dropDownListViewStyle);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setId(R.id.omnibox_suggestions_dropdown);

        // By default RecyclerViews come with item animators.
        setItemAnimator(null);
        addItemDecoration(new SuggestionHorizontalDivider(context));

        mLayoutScrollListener = new SuggestionLayoutScrollListener(context);
        setLayoutManager(mLayoutScrollListener);
        mSelectionController = new RecyclerViewSelectionController(mLayoutScrollListener);
        addOnChildAttachStateChangeListener(mSelectionController);

        final Resources resources = context.getResources();
        mBaseBottomPadding =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        int paddingTop =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_top);
        this.setPaddingRelative(0, paddingTop, 0, mBaseBottomPadding);

        if (OmniboxFeatures.sAsyncViewInflation.isEnabled()) {
            setRecycledViewPool(new PreWarmingRecycledViewPool(mAdapter, context));
        }
    }

    /**
     * Override the visuals of the Omnibox. This method is particularly relevant for SearchActivity,
     * which presents Phone-style omnibox even when running on Tablets.
     *
     * @param shouldForce whether Omnibox should be forced to use Phone-style visuals
     */
    public void forcePhoneStyleOmnibox(boolean shouldForce) {
        if (!shouldForce
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                && getContext().getResources().getConfiguration().screenWidthDp
                        >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP) {
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
    public @NonNull ViewGroup getViewGroup() {
        return this;
    }

    /** Clean up resources and remove observers installed by this class. */
    public void destroy() {
        getRecycledViewPool().clear();
        mGestureObserver = null;
        mHeightChangeListener = null;
    }

    /**
     * Sets the observer for that the user is interacting with an item on the Suggestions list..
     *
     * @param observer an observer of this gesture.
     */
    public void setGestureObserver(@NonNull OmniboxSuggestionsDropdown.GestureObserver observer) {
        mGestureObserver = observer;
    }

    /**
     * Sets the listener for changes of the suggestion list's height. The height may change as a
     * result of eg. soft keyboard popping up.
     *
     * @param listener A listener will receive the new height of the suggestion list in pixels.
     */
    public void setHeightChangeListener(@NonNull Callback<Integer> listener) {
        mHeightChangeListener = listener;
    }

    /** Resets selection typically in response to changes to the list. */
    public void resetSelection() {
        mSelectionController.resetSelection();
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
    public void onChildAttachedToWindow(@NonNull View child) {
        child.setAlpha(mChildAlpha);
        if (mChildVerticalTranslation != 0.0f) {
            child.setTranslationY(mChildVerticalTranslation);
        }
    }

    @Override
    public void onChildDetachedFromWindow(@NonNull View child) {
        child.setTranslationY(0.0f);
        child.setAlpha(1.0f);
    }

    /** Resests the tracked keyboard shown state to properly respond to scroll events. */
    void resetKeyboardShownState() {
        mLayoutScrollListener.resetKeyboardShownState();
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
        final LayoutManager manager = getLayoutManager();
        manager.scrollToPosition(index);
        return manager.findViewByPosition(index);
    }

    /**
     * Update the suggestion popup background to reflect the current state.
     *
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    public void refreshPopupBackground(@BrandedColorScheme int brandedColorScheme) {
        int color =
                OmniboxResourceProvider.getSuggestionsDropdownBackgroundColorForColorScheme(
                        getContext(), brandedColorScheme);

        if (!isHardwareAccelerated()) {
            // When HW acceleration is disabled, changing mSuggestionList' items somehow erases
            // mOmniboxResultsContainer' background from the area not covered by
            // mSuggestionList. To make sure mOmniboxResultsContainer is always redrawn, we make
            // list background color slightly transparent. This makes mSuggestionList.isOpaque()
            // to return false, and forces redraw of the parent view (mOmniboxResultsContainer).
            if (Color.alpha(color) == 255) {
                color = Color.argb(254, Color.red(color), Color.green(color), Color.blue(color));
            }
        }
        setBackground(new ColorDrawable(color));
    }

    @Override
    public void setAdapter(@NonNull Adapter adapter) {
        mAdapter = (OmniboxSuggestionsDropdownAdapter) adapter;
        super.setAdapter(mAdapter);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        boolean isTablet = mEmbedder.map(e -> e.isTablet()).orElse(false);

        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Measure");
                TimingMetric metric = OmniboxMetrics.recordSuggestionListMeasureTime();
                TimingMetric metric2 = OmniboxMetrics.recordSuggestionListMeasureWallTime()) {
            maybeUpdateLayoutParams(mOmniboxAlignment.top);
            int availableViewportHeight = mOmniboxAlignment.height;
            int desiredWidth = mOmniboxAlignment.width;
            adjustHorizontalPosition();
            notifyObserversIfViewportHeightChanged(availableViewportHeight);

            widthMeasureSpec = MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY);
            heightMeasureSpec =
                    MeasureSpec.makeMeasureSpec(
                            availableViewportHeight,
                            isTablet ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            if (isTablet) {
                setRoundBottomCorners(
                        getMeasuredHeight() < availableViewportHeight
                                || !KeyboardVisibilityDelegate.getInstance()
                                        .isKeyboardShowing(getContext(), this));
            }
        }
    }

    private void maybeUpdateLayoutParams(int topMargin) {
        // Update the layout params to ensure the parent correctly positions the suggestions
        // under the anchor view.
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        if (layoutParams != null && layoutParams instanceof ViewGroup.MarginLayoutParams) {
            ((ViewGroup.MarginLayoutParams) layoutParams).topMargin = topMargin;
        }
    }

    private void notifyObserversIfViewportHeightChanged(int availableViewportHeight) {
        if (availableViewportHeight == mListViewMaxHeight) return;

        mListViewMaxHeight = availableViewportHeight;
        if (mHeightChangeListener != null) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Detect if there was another change since this task posted.
                        // This indicates a subsequent task being posted too.
                        if (mListViewMaxHeight != availableViewportHeight) return;
                        // Detect if the new height is the same as previously broadcasted.
                        // The two checks (one above and one below) allow us to detect quick
                        // A->B->A transitions and suppress the broadcasts.
                        if (mLastBroadcastedListViewMaxHeight == availableViewportHeight) return;
                        if (mHeightChangeListener == null) return;

                        mHeightChangeListener.onResult(availableViewportHeight);
                        mLastBroadcastedListViewMaxHeight = availableViewportHeight;
                    });
        }
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

        if (keyCode == KeyEvent.KEYCODE_TAB) {
            boolean maybeProcessed = super.onKeyDown(keyCode, event);
            if (maybeProcessed) return true;
            if (event.isShiftPressed()) {
                return mSelectionController.selectPreviousItem();
            }
            return mSelectionController.selectNextItem();
        }

        if (KeyNavigationUtil.isGoDown(event)) {
            mSelectionController.selectNextItem();
            return true;
        } else if (KeyNavigationUtil.isGoUp(event)) {
            mSelectionController.selectPreviousItem();
            return true;
        } else if (KeyNavigationUtil.isEnter(event)) {
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // Consume mouse events to ensure clicks do not bleed through to sibling views that
        // are obscured by the list.  crbug.com/968414
        int action = event.getActionMasked();
        boolean shouldIgnoreGenericMotionEvent =
                (event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                        && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE
                        && (action == MotionEvent.ACTION_BUTTON_PRESS
                                || action == MotionEvent.ACTION_BUTTON_RELEASE);
        return shouldIgnoreGenericMotionEvent || super.onGenericMotionEvent(event);
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

    /**
     * Sets the embedder for the list view.
     *
     * @param embedder the embedder of this list.
     */
    public void setEmbedder(@NonNull OmniboxSuggestionsDropdownEmbedder embedder) {
        // Don't reset the current value of `mOmniboxAlignment`, and don't read the value from newly
        // installed embedder to ensure the `onOmniboxAlignmentChanged` does the right thing when we
        // install our observers.
        mEmbedder = Optional.of(embedder);
    }

    /**
     * Respond to Omnibox session state change.
     *
     * @param urlHasFocus whether URL has focus (signaling the session is active)
     */
    /* package */ void onOmniboxSessionStateChange(boolean urlHasFocus) {
        if (urlHasFocus) {
            installAlignmentObserver();
        } else {
            removeAlignmentObserver();
        }
    }

    private void installAlignmentObserver() {
        mEmbedder.ifPresent(
                e -> {
                    e.onAttachedToWindow();
                    mOmniboxAlignment = e.addAlignmentObserver(mOmniboxAlignmentObserver);
                });
    }

    private void removeAlignmentObserver() {
        mEmbedder.ifPresent(
                e -> {
                    e.onDetachedFromWindow();
                    e.removeAlignmentObserver(mOmniboxAlignmentObserver);
                });

        if (!OmniboxFeatures.shouldPreWarmRecyclerViewPool()) {
            getRecycledViewPool().clear();
        }
    }

    private void onOmniboxAlignmentChanged(@NonNull OmniboxAlignment omniboxAlignment) {
        boolean isOnlyHorizontalDifference =
                omniboxAlignment.isOnlyHorizontalDifference(mOmniboxAlignment);
        boolean isWidthDifference = omniboxAlignment.doesWidthDiffer(mOmniboxAlignment);
        mOmniboxAlignment = omniboxAlignment;
        this.setPaddingRelative(
                getPaddingStart(),
                getPaddingTop(),
                getPaddingEnd(),
                mBaseBottomPadding + mOmniboxAlignment.paddingBottom);

        if (isOnlyHorizontalDifference) {
            adjustHorizontalPosition();
            return;
        } else if (isWidthDifference) {
            // If our width has changed, we may have views in our pool that are now the wrong width.
            // Recycle them by calling swapAdapter() to avoid showing views of the wrong size.
            swapAdapter(mAdapter, true);
            Configuration configuration = getContext().getResources().getConfiguration();
            setClipToOutline(
                    configuration.screenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP);
            BaseSuggestionViewBinder.resetCachedResources();
        }
        if (isInLayout()) {
            // requestLayout doesn't behave predictably in the middle of a layout pass. Even if it
            // does trigger a second layout pass, measurement caches aren't properly reset,
            // resulting in stale sizing. Absent a way to abort the current pass and start over the
            // simplest solution is to wait until the current pass is over to request relayout.
            PostTask.postTask(
                    TaskTraits.UI_USER_VISIBLE,
                    () -> {
                        ViewUtils.requestLayout(
                                OmniboxSuggestionsDropdown.this,
                                "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
                    });
        } else {
            ViewUtils.requestLayout(
                    (View) OmniboxSuggestionsDropdown.this,
                    "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
        }
    }

    private void adjustHorizontalPosition() {
        // Set our left edge using translation x. This avoids needing to relayout (like setting
        // a left margin would) and is less risky than calling View#setLeft(), which is intended
        // for use by the layout system.
        setTranslationX(mOmniboxAlignment.left);
    }

    private void setRoundBottomCorners(boolean roundBottomCorners) {
        ViewOutlineProvider outlineProvider = getOutlineProvider();
        if (!(outlineProvider instanceof RoundedCornerOutlineProvider)) return;

        RoundedCornerOutlineProvider roundedCornerOutlineProvider =
                (RoundedCornerOutlineProvider) outlineProvider;
        roundedCornerOutlineProvider.setRoundingEdges(true, true, true, roundBottomCorners);
    }

    public void emitWindowContentChanged() {
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    announceForAccessibility(
                            getContext()
                                    .getString(
                                            R.string.accessibility_omnibox_suggested_items,
                                            mAdapter.getItemCount()));
                },
                LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS);
    }

    @VisibleForTesting
    SuggestionLayoutScrollListener getLayoutScrollListener() {
        return mLayoutScrollListener;
    }
}
