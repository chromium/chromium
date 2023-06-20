// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownEmbedder.OmniboxAlignment;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.browser.util.KeyNavigationUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A widget for showing a list of omnibox suggestions. */
public class OmniboxSuggestionsDropdown extends RecyclerView {
    private static final long DEFERRED_INITIAL_SHRINKING_LAYOUT_FROM_IME_DURATION_MS = 300;
    /**
     * Used to defer the accessibility announcement for list content.
     * This makes core difference when the list is first shown up, when the interaction with the
     * Omnibox and presence of virtual keyboard may actually cause throttling of the Accessibility
     * events.
     */
    private static final long LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS = 300;

    private final int mStandardBgColor;
    private final int mIncognitoBgColor;

    private final Rect mTempRect = new Rect();
    private final SuggestionLayoutScrollListener mLayoutScrollListener;

    private @Nullable OmniboxSuggestionsDropdownAdapter mAdapter;
    private @Nullable OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private @Nullable GestureObserver mGestureObserver;
    private @Nullable Callback<Integer> mHeightChangeListener;
    private @Nullable Runnable mSuggestionDropdownScrollListener;
    private @Nullable Runnable mSuggestionDropdownOverscrolledToTopListener;
    private @NonNull OmniboxAlignment mOmniboxAlignment = OmniboxAlignment.UNSPECIFIED;

    private int mListViewMaxHeight;
    private int mLastBroadcastedListViewMaxHeight;
    private @Nullable Callback<OmniboxAlignment> mOmniboxAlignmentObserver;

    @IntDef({InitialResizeState.WAITING_FOR_FIRST_MEASURE, InitialResizeState.WAITING_FOR_SHRINKING,
            InitialResizeState.IGNORING_SHRINKING, InitialResizeState.HANDLED_INITIAL_SIZING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface InitialResizeState {
        int WAITING_FOR_FIRST_MEASURE = 0;
        int WAITING_FOR_SHRINKING = 1;
        int IGNORING_SHRINKING = 2;
        int HANDLED_INITIAL_SIZING = 3;
    }

    private @InitialResizeState int mInitialResizeState =
            InitialResizeState.WAITING_FOR_FIRST_MEASURE;
    private int mWidthMeasureSpec;
    private int mHeightMeasureSpec;

    /**
     * Interface that will receive notifications when the user is interacting with an item on the
     * Suggestions list.
     */
    public interface GestureObserver {
        /**
         * Notify that the user is interacting with an item on the Suggestions list.
         *
         * @param isGestureUp Whether user pressed (false) or depressed (true) the element on the
         *         list.
         * @param timestamp The timestamp associated with the event.
         */
        void onGesture(boolean isGestureUp, long timestamp);
    }

    /** Scroll manager that propagates scroll event notification to registered observers. */
    @VisibleForTesting
    /* package */ class SuggestionLayoutScrollListener extends LinearLayoutManager {
        private boolean mLastKeyboardShownState;

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
         * - Upon scroll down from the top, if the distance scrolled is same as distance requested
         *   (= the list has enough content to respond to the request), hide the keyboard and
         *   suppress the scroll action by reporting 0 as the resulting scroll distance.
         * - Upon scroll up to the top, if the distance scrolled is shorter than the distance
         *   requested (= the list has reached the top), show the keyboard.
         * - In all other cases, take no action.
         *
         * The code reports 0 if and only if the keyboard state transitions from "shown" to
         * "hidden".
         *
         * The logic remembers the last requested keyboard state, so that the keyboard is not
         * repeatedly called up or requested to be hidden.
         *
         * @param resultingDeltaY The scroll distance by which the LayoutManager intends to scroll.
         *         Negative values indicate scroll up, positive values indicate scroll down.
         * @param requestedDeltaY The scroll distance requested by the user via gesture.
         *         Negative values indicate scroll up, positive values indicate scroll down.
         * @return Value of resultingDeltaY, if scroll is permitted, or 0 when it is suppressed.
         */
        @VisibleForTesting
        /* package */ int updateKeyboardVisibilityAndScroll(
                int resultingDeltaY, int requestedDeltaY) {
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
            // There's no need to dismiss the keyboard in any of these cases.
            if (resultingDeltaY < requestedDeltaY) return resultingDeltaY;
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
         * Reset the internal keyboard state.
         * This needs to be called either when the SuggestionsDropdown is hidden or shown again
         * to reflect either the end of the current or beginning of the next interaction
         * session.
         */
        @VisibleForTesting
        /* package */ void resetKeyboardShownState() {
            mLastKeyboardShownState = true;
        }
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsDropdown(@NonNull Context context, RecycledViewPool recycledViewPool) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setRecycledViewPool(recycledViewPool);
        setId(R.id.omnibox_suggestions_dropdown);

        // By default RecyclerViews come with item animators.
        setItemAnimator(null);

        mLayoutScrollListener = new SuggestionLayoutScrollListener(context);
        setLayoutManager(mLayoutScrollListener);

        boolean shouldShowModernizeVisualUpdate =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(context);
        final Resources resources = context.getResources();
        int paddingBottom =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);

        mStandardBgColor = shouldShowModernizeVisualUpdate
                ? ChromeColors.getSurfaceColor(
                        context, R.dimen.omnibox_suggestion_dropdown_bg_elevation)
                : ChromeColors.getDefaultThemeColor(context, false);
        int incognitoBgColorRes = ChromeFeatureList.sBaselineGm3SurfaceColors.isEnabled()
                ? R.color.default_bg_color_dark_elev_1_gm3_baseline
                : R.color.omnibox_dropdown_bg_incognito;
        mIncognitoBgColor = shouldShowModernizeVisualUpdate
                ? context.getColor(incognitoBgColorRes)
                : ChromeColors.getDefaultThemeColor(context, true);
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
        mSuggestionDropdownScrollListener = null;
        mSuggestionDropdownOverscrolledToTopListener = null;
    }

    /**
     * Sets the observer for that the user is interacting with an item on the Suggestions list..
     * @param observer an observer of this gesture.
     */
    public void setGestureObserver(@NonNull OmniboxSuggestionsDropdown.GestureObserver observer) {
        mGestureObserver = observer;
    }

    /**
     * Sets the listener for changes of the suggestion list's height.
     * The height may change as a result of eg. soft keyboard popping up.
     *
     * @param listener A listener will receive the new height of the suggestion list in pixels.
     */
    public void setHeightChangeListener(@NonNull Callback<Integer> listener) {
        mHeightChangeListener = listener;
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

    /** Resets selection typically in response to changes to the list. */
    public void resetSelection() {
        if (mAdapter == null) return;
        mAdapter.resetSelection();
    }

    /** Resests the tracked keyboard shown state to properly respond to scroll events. */
    void resetKeyboardShownState() {
        mLayoutScrollListener.resetKeyboardShownState();
    }

    /** @return The number of items in the list. */
    public int getDropdownItemViewCountForTest() {
        if (mAdapter == null) return 0;
        return mAdapter.getItemCount();
    }

    /** @return The Suggestion view at specific index. */
    public @Nullable View getDropdownItemViewForTest(int index) {
        final LayoutManager manager = getLayoutManager();
        manager.scrollToPosition(index);
        return manager.findViewByPosition(index);
    }

    /**
     * Update the suggestion popup background to reflect the current state.
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    public void refreshPopupBackground(@BrandedColorScheme int brandedColorScheme) {
        int color = brandedColorScheme == BrandedColorScheme.INCOGNITO ? mIncognitoBgColor
                                                                       : mStandardBgColor;
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
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        mEmbedder.onAttachedToWindow();
        mInitialResizeState = InitialResizeState.WAITING_FOR_FIRST_MEASURE;
        mOmniboxAlignmentObserver = this::onOmniboxAlignmentChanged;
        mOmniboxAlignment = mEmbedder.addAlignmentObserver(mOmniboxAlignmentObserver);
        resetSelection();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mEmbedder.onDetachedFromWindow();
        mOmniboxAlignment = OmniboxAlignment.UNSPECIFIED;
        if (!OmniboxFeatures.shouldPreWarmRecyclerViewPool()) {
            getRecycledViewPool().clear();
        }
        mAdapter.recordSessionMetrics();
        mEmbedder.removeAlignmentObserver(mOmniboxAlignmentObserver);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Measure");
                TimingMetric metric = OmniboxMetrics.recordSuggestionListMeasureTime()) {
            OmniboxAlignment omniboxAlignment = mEmbedder.getCurrentAlignment();
            maybeUpdateLayoutParams(omniboxAlignment.top);
            boolean useAlignmentSpecifiedHeight = OmniboxFeatures.omniboxConsumesImeInsets();
            int availableViewportHeight = useAlignmentSpecifiedHeight
                    ? omniboxAlignment.height
                    : calculateAvailableViewportHeight() - omniboxAlignment.top;
            int desiredWidth = omniboxAlignment.width;
            adjustHorizontalPosition();
            // Suppress the initial requests to shrink the viewport of the omnibox suggestion
            // dropdown. The viewport will decrease when the keyboard is triggered, but the request
            // to resize happens when the keyboard starts showing before it has had the chance to
            // animate in. Because the resizing is triggered early, the dropdown shrinks earlier
            // then the keyboard is fully visible, which leaves a hole in the UI showing the content
            // where the keyboard will eventually go.
            //
            // The work around is to suppress these initial shrinking layout requests and defer them
            // for enough time for the keyboard to hopefully be visible.
            //
            // This does not use getMeasuredHeight() as a means of comparison against the available
            // viewport because on tablets the measured height can be smaller than the viewport as
            // tablets use AT_MOST for the measure spec vs EXACTLY on phones.
            // This logic is moot when we use alignment-specified height; the deferral of keyboard
            // height changes is handled for us in that case.
            if (!useAlignmentSpecifiedHeight) {
                if ((mInitialResizeState == InitialResizeState.WAITING_FOR_SHRINKING
                            || mInitialResizeState == InitialResizeState.IGNORING_SHRINKING)
                        && availableViewportHeight < mListViewMaxHeight
                        && getMeasuredWidth() == desiredWidth) {
                    super.onMeasure(mWidthMeasureSpec, mHeightMeasureSpec);
                    if (mInitialResizeState == InitialResizeState.IGNORING_SHRINKING) return;

                    mInitialResizeState = InitialResizeState.IGNORING_SHRINKING;
                    PostTask.postDelayedTask(TaskTraits.UI_USER_BLOCKING, () -> {
                        if (mInitialResizeState != InitialResizeState.IGNORING_SHRINKING) return;
                        ViewUtils.requestLayout(this, "OmniboxSuggestionsDropdown.onMeasure");
                        mInitialResizeState = InitialResizeState.HANDLED_INITIAL_SIZING;
                    }, DEFERRED_INITIAL_SHRINKING_LAYOUT_FROM_IME_DURATION_MS);
                    return;
                } else if (mInitialResizeState == InitialResizeState.IGNORING_SHRINKING) {
                    // The dimensions changed in an unexpected way (either by increasing height or
                    // a change in width), so just mark the initial sizing as completed and accept
                    // the new measurements and suppress the pending posted layout request.
                    mInitialResizeState = InitialResizeState.HANDLED_INITIAL_SIZING;
                }
            }
            notifyObserversIfViewportHeightChanged(availableViewportHeight);

            mWidthMeasureSpec = MeasureSpec.makeMeasureSpec(desiredWidth, MeasureSpec.EXACTLY);
            mHeightMeasureSpec = MeasureSpec.makeMeasureSpec(availableViewportHeight,
                    mEmbedder.isTablet() ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY);
            super.onMeasure(mWidthMeasureSpec, mHeightMeasureSpec);
            if (mInitialResizeState == InitialResizeState.WAITING_FOR_FIRST_MEASURE) {
                mInitialResizeState = InitialResizeState.WAITING_FOR_SHRINKING;
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

    private int calculateAvailableViewportHeight() {
        mEmbedder.getWindowDelegate().getWindowVisibleDisplayFrame(mTempRect);
        return mTempRect.height();
    }

    private void notifyObserversIfViewportHeightChanged(int availableViewportHeight) {
        if (availableViewportHeight == mListViewMaxHeight) return;

        mListViewMaxHeight = availableViewportHeight;
        if (mHeightChangeListener != null) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> {
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
                TimingMetric metric = OmniboxMetrics.recordSuggestionListLayoutTime()) {
            super.onLayout(changed, l, t, r, b);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        View selectedView = mAdapter.getSelectedView();
        if (selectedView != null && selectedView.onKeyDown(keyCode, event)) {
            return true;
        }

        int selectedPosition = mAdapter.getSelectedViewIndex();
        if (KeyNavigationUtil.isGoDown(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition + 1);
        } else if (KeyNavigationUtil.isGoUp(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition - 1);
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
        if ((eventType == MotionEvent.ACTION_UP || eventType == MotionEvent.ACTION_DOWN)
                && mGestureObserver != null) {
            mGestureObserver.onGesture(eventType == MotionEvent.ACTION_UP, ev.getEventTime());
        }
        return super.dispatchTouchEvent(ev);
    }

    /**
     * Sets the embedder for the list view.
     * @param embedder the embedder of this list.
     */
    public void setEmbedder(@NonNull OmniboxSuggestionsDropdownEmbedder embedder) {
        assert mEmbedder == null;
        mEmbedder = embedder;
        mOmniboxAlignment = mEmbedder.getCurrentAlignment();
    }

    private void onOmniboxAlignmentChanged(@NonNull OmniboxAlignment omniboxAlignment) {
        boolean isOnlyHorizontalDifference =
                omniboxAlignment.isOnlyHorizontalDifference(mOmniboxAlignment);
        boolean isWidthDifference = omniboxAlignment.doesWidthDiffer(mOmniboxAlignment);
        mOmniboxAlignment = omniboxAlignment;
        if (isOnlyHorizontalDifference) {
            adjustHorizontalPosition();
            return;
        } else if (isWidthDifference) {
            // If our width has changed, we may have views in our pool that are now the wrong width.
            // Recycle them by calling swapAdapter() to avoid showing views of the wrong size.
            swapAdapter(mAdapter, true);
        }
        if (isInLayout()) {
            // requestLayout doesn't behave predictably in the middle of a layout pass. Even if it
            // does trigger a second layout pass, measurement caches aren't properly reset,
            // resulting in stale sizing. Absent a way to abort the current pass and start over the
            // simplest solution is to wait until the current pass is over to request relayout.
            PostTask.postTask(TaskTraits.UI_USER_VISIBLE, () -> {
                ViewUtils.requestLayout(OmniboxSuggestionsDropdown.this,
                        "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
            });
        } else {
            ViewUtils.requestLayout((View) OmniboxSuggestionsDropdown.this,
                    "OmniboxSuggestionsDropdown.onOmniboxAlignmentChanged");
        }
    }

    private void adjustHorizontalPosition() {
        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(getContext())) {
            // Set our left edge using translation x. This avoids needing to relayout (like setting
            // a left margin would) and is less risky than calling View#setLeft(), which is intended
            // for use by the layout system.
            setTranslationX(mOmniboxAlignment.left);
        } else {
            setPadding(mOmniboxAlignment.paddingLeft, getPaddingTop(),
                    mOmniboxAlignment.paddingRight, getPaddingBottom());
        }
    }

    public void emitWindowContentChanged() {
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> {
            announceForAccessibility(getContext().getString(
                    R.string.accessibility_omnibox_suggested_items, mAdapter.getItemCount()));
        }, LIST_COMPOSITION_ACCESSIBILITY_ANNOUNCEMENT_DELAY_MS);
    }

    @VisibleForTesting
    public int getStandardBgColor() {
        return mStandardBgColor;
    }

    @VisibleForTesting
    public int getIncognitoBgColor() {
        return mIncognitoBgColor;
    }

    @VisibleForTesting
    SuggestionLayoutScrollListener getLayoutScrollListener() {
        return mLayoutScrollListener;
    }
}
