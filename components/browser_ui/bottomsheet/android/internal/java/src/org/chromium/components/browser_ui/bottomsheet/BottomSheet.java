// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;

/**
 * This class defines the bottom sheet that has multiple states and a persistently showing toolbar.
 * Namely, the states are:
 * - PEEK: Only the toolbar is visible at the bottom of the screen.
 * - HALF: The sheet is expanded to consume around half of the screen.
 * - FULL: The sheet is expanded to its full height.
 *
 * All the computation in this file is based off of the bottom of the screen instead of the top
 * for simplicity. This means that the bottom of the screen is 0 on the Y axis.
 */
class BottomSheet extends FrameLayout
        implements BottomSheetSwipeDetector.SwipeableBottomSheet, View.OnLayoutChangeListener {
    private static final String TAG = "BottomSheet";

    /** Duration for transition to {@link SheetState#FULL}. */
    private static final int ANIMATION_DURATION_EXPAND_MS = 350;

    /** Duration for transition from {@link SheetState#FULL}. */
    private static final int ANIMATION_DURATION_SHRINK_MS = 250;

    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. This is the value used when there are 3 active states. A smaller value here means
     * a smaller swipe is needed to move the sheet around.
     */
    private static final float THRESHOLD_TO_NEXT_STATE_3 = 0.4f;

    /** This is similar to {@link #THRESHOLD_TO_NEXT_STATE_3} but for 2 states instead of 3. */
    private static final float THRESHOLD_TO_NEXT_STATE_2 = 0.3f;

    /** The height ratio for the sheet in the SheetState.HALF state. */
    private static final float HALF_HEIGHT_RATIO = 0.75f;

    /** The desired height of a content that has just been shown or whose height was invalidated. */
    private static final float HEIGHT_UNSPECIFIED = -1.0f;

    /** A means of reporting an exception/stack without crashing. */
    private static Callback<Throwable> sExceptionReporter;

    /** A flag to force the small screen state of the bottom sheet. */
    private static Boolean sIsSmallScreenForTesting;

    /** The list of observers of this sheet. */
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    /** The visible rect for the screen taking the keyboard into account. */
    private final Rect mVisibleViewportRect = new Rect();

    /** An out-array for use with getLocationInWindow to prevent constant allocations. */
    private final int[] mCachedLocation = new int[2];

    /** The minimum distance between half and full states to allow the half state. */
    private final float mMinHalfFullDistance;

    /** The view that contains the sheet. */
    private ViewGroup mSheetContainer;

    /** For detecting scroll and fling events on the bottom sheet. */
    private BottomSheetSwipeDetector mGestureDetector;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private ValueAnimator mSettleAnimator;

    /** The width of the view that contains the bottom sheet. */
    private int mContainerWidth;

    /** The height of the view that contains the bottom sheet. */
    private int mContainerHeight;

    /** The desired height of the current content view. */
    private float mContentDesiredHeight = HEIGHT_UNSPECIFIED;

    /**
     * The current offset of the sheet from the bottom of the screen in px. This does not include
     * added offset from the scrolling of the browser controls which allows the sheet's toolbar to
     * show and hide in-sync with the top toolbar.
     */
    private float mCurrentOffsetPx;

    /** The current state that the sheet is in. */
    @SheetState private int mCurrentState = SheetState.HIDDEN;

    /** The target sheet state. This is the state that the sheet is currently moving to. */
    @SheetState private int mTargetState = SheetState.NONE;

    /** While scrolling, this holds the state the scrolling started in. Otherwise, it's NONE. */
    @SheetState int mScrollingStartState = SheetState.NONE;

    /** A handle to the content being shown by the sheet. */
    @Nullable protected BottomSheetContent mSheetContent;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /**
     * The last offset ratio sent to observers of onSheetOffsetChanged(). This is used to ensure the
     * min and max values are provided at least once (0 and 1).
     */
    private float mLastOffsetRatioSent;

    /** The FrameLayout used to hold the bottom sheet toolbar. */
    private TouchRestrictingFrameLayout mToolbarHolder;

    /** Whether the {@link BottomSheet} and its children should react to touch events. */
    private boolean mIsTouchEnabled;

    /** Whether the sheet is currently open. */
    private boolean mIsSheetOpen;

    /** Whether {@link #destroy()} has been called. **/
    private boolean mIsDestroyed;

    /** The ratio in the range [0, 1] that the browser controls are hidden. */
    private float mBrowserControlsHiddenRatio;

    /** Whether or not always use the full width of the container. */
    private boolean mAlwaysFullWidth;

    /** The supplier of the bottom inset when edge to edge is enabled. */
    private Supplier<Integer> mEdgeToEdgeBottomInsetSupplier = () -> 0;

    /** The last recorded app header height, in px. */
    private int mAppHeaderHeight;

    /**
     * A view used to render a shadow behind the sheet and extends outside the bounds of its parent
     * view.
     */
    public static class ShadowLayerView extends View {
        /** The length of the shadow in any direction. */
        private int mShadowLength;

        /** Constructor to inflate from XML. */
        public ShadowLayerView(Context context, AttributeSet atts) {
            super(context, atts);
            mShadowLength =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
            setTranslationX((LocalizationUtils.isLayoutRtl() ? 1 : -1) * mShadowLength);
            setTranslationY(-mShadowLength);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(widthMeasureSpec) + 2 * mShadowLength,
                            MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(heightMeasureSpec) + mShadowLength,
                            MeasureSpec.EXACTLY));
        }
    }

    @Override
    public boolean shouldGestureMoveSheet(MotionEvent initialEvent, MotionEvent currentEvent) {
        // If the sheet is scrolling off-screen or in the process of hiding, gestures should not
        // affect it.
        if (getOffsetFromBrowserControls() > 0 || isHiding()) {
            return false;
        }

        // If the sheet is already open, or an accessibility service that can perform gestures or
        // uses touch exploration is enabled, there is no need to restrict the swipe area.
        if (isSheetOpen()
                || AccessibilityState.isPerformGesturesEnabled()
                || AccessibilityState.isTouchExplorationEnabled()) {
            return true;
        }

        float startX = mVisibleViewportRect.left;
        float endX = getWidth() + mVisibleViewportRect.left;
        return currentEvent.getRawX() > startX && currentEvent.getRawX() < endX;
    }

    /**
     * Constructor for inflation from XML.
     * @param context An Android context.
     * @param atts The XML attributes.
     */
    public BottomSheet(Context context, AttributeSet atts) {
        super(context, atts);

        mMinHalfFullDistance =
                getResources().getDimensionPixelSize(R.dimen.bottom_sheet_min_full_half_distance);

        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;
    }

    /** @param reporter A means of reporting an exception without crashing. */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        sExceptionReporter = reporter;
    }

    /** Called when the activity containing the {@link BottomSheet} is destroyed. */
    void destroy() {
        mIsDestroyed = true;
        mIsTouchEnabled = false;
        mObservers.clear();
        endAnimations();
    }

    /** Immediately end all animations and null the animators. */
    void endAnimations() {
        if (mSettleAnimator != null) mSettleAnimator.end();
        mSettleAnimator = null;
    }

    /** @return Whether the sheet is in the process of hiding. */
    boolean isHiding() {
        return mSettleAnimator != null && mTargetState == SheetState.HIDDEN;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        if (!isTouchEventInUsableArea(e) && e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            return false;
        }

        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        if (isHiding()) return false;

        return mGestureDetector.onInterceptTouchEvent(e);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (!isTouchEventInUsableArea(e) && e.getActionMasked() == MotionEvent.ACTION_DOWN) {
            return false;
        }

        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        mGestureDetector.onTouchEvent(e);

        return true;
    }

    @Override
    public boolean onHoverEvent(MotionEvent event) {
        // https://crbug.com/1297267 Consume hover events to prevent talkback from reading items
        // behind the bottom sheet, in particular when the client has its own scrim lifecycle.
        super.onHoverEvent(event);
        return true;
    }

    /**
     * Adds layout change listeners to the views that the bottom sheet depends on. Namely the
     * heights of the root view and control container are important as they are used in many of the
     * calculations in this class.
     *
     * @param window Android window for getting insets.
     * @param keyboardDelegate Delegate for hiding the keyboard.
     * @param alwaysFullWidth Whether bottom sheet is always full-width.
     * @param edgeToEdgeBottomInsetSupplier The supplier of the bottom inset in DP when e2e is on.
     * @param appHeaderHeight The app header height, in px.
     */
    public void init(
            Window window,
            KeyboardVisibilityDelegate keyboardDelegate,
            boolean alwaysFullWidth,
            @NonNull Supplier<Integer> edgeToEdgeBottomInsetSupplier,
            int appHeaderHeight) {
        mEdgeToEdgeBottomInsetSupplier = edgeToEdgeBottomInsetSupplier;
        mSheetContainer = (ViewGroup) getParent();
        onAppHeaderHeightChanged(appHeaderHeight);

        mToolbarHolder =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_toolbar_container);

        mBottomSheetContentContainer =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_content);
        mBottomSheetContentContainer.setBottomSheet(this);

        mContainerWidth = mSheetContainer.getWidth();
        mContainerHeight = mSheetContainer.getHeight();
        mAlwaysFullWidth = alwaysFullWidth;

        sizeAndPositionSheetInParent();

        // Listen to height changes on the root.
        mSheetContainer.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    private int mPreviousKeyboardHeight;

                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // Compute the new height taking the keyboard into account.
                        // TODO(mdjones): Share this logic with LocationBarLayout: crbug.com/725725.
                        int previousWidth = mContainerWidth;
                        int previousHeight = mContainerHeight;
                        mContainerWidth = right - left;
                        mContainerHeight = bottom - top;

                        if (previousWidth != mContainerWidth
                                || previousHeight != mContainerHeight) {
                            if (!isHalfStateEnabled()) {
                                if (mCurrentState == SheetState.HALF) {
                                    setSheetState(SheetState.FULL, false);
                                } else if (mCurrentState == SheetState.SCROLLING
                                        && mTargetState == SheetState.HALF) {
                                    // Let the animation resume to the full height.
                                    mTargetState = SheetState.FULL;
                                }
                            }
                            invalidateContentDesiredHeight();
                            sizeAndPositionSheetInParent();
                        }

                        int heightMinusKeyboard = (int) mContainerHeight;
                        int keyboardHeight = 0;

                        // Reset mVisibleViewportRect regardless of sheet open state as it is used
                        // outside of calculating the keyboard height.
                        window.getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
                        if (isSheetOpen()) {
                            int decorHeight = window.getDecorView().getHeight();
                            heightMinusKeyboard =
                                    Math.min(decorHeight, mVisibleViewportRect.height());
                            keyboardHeight =
                                    Math.max(0, (int) (mContainerHeight - heightMinusKeyboard));
                        }

                        if (keyboardHeight != mPreviousKeyboardHeight) {
                            // If the keyboard height changed, recompute the padding for the content
                            // area.
                            // This shrinks the content size while retaining the default background
                            // color where the keyboard is appearing. If the sheet is not showing,
                            // resize the sheet to its default state.
                            mBottomSheetContentContainer.setPadding(
                                    mBottomSheetContentContainer.getPaddingLeft(),
                                    mBottomSheetContentContainer.getPaddingTop(),
                                    mBottomSheetContentContainer.getPaddingRight(),
                                    keyboardHeight);
                        }

                        if (previousHeight != mContainerHeight
                                || mPreviousKeyboardHeight != keyboardHeight) {
                            // If we are in the middle of a touch event stream (i.e. scrolling while
                            // keyboard is up) don't set the sheet state. Instead allow the gesture
                            // detector to position the sheet and make sure the keyboard hides.
                            if (mGestureDetector.isScrolling() && keyboardDelegate != null) {
                                keyboardDelegate.hideKeyboard(BottomSheet.this);
                            } else {
                                if (mTargetState != SheetState.NONE) {
                                    cancelAnimation();
                                    createSettleAnimation(mTargetState, StateChangeReason.NONE);
                                } else {
                                    endAnimations();
                                    setSheetState(mCurrentState, false);
                                }
                            }
                        }

                        mPreviousKeyboardHeight = keyboardHeight;
                    }
                });

        // Listen to height changes on the toolbar.
        mToolbarHolder.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // Make sure the size of the layout actually changed.
                        if (bottom - top == oldBottom - oldTop
                                && right - left == oldRight - oldLeft) {
                            return;
                        }

                        if (!mGestureDetector.isScrolling() && isRunningSettleAnimation()) return;

                        setSheetState(mCurrentState, false);
                    }
                });

        mSheetContainer.removeView(this);
    }

    /** @param ratio The current browser controls hidden ratio. */
    void setBrowserControlsHiddenRatio(float ratio) {
        mBrowserControlsHiddenRatio = ratio;

        if (getSheetState() == SheetState.HIDDEN) return;
        int state = isHalfStateEnabled() ? SheetState.HALF : SheetState.PEEK;
        if (getCurrentOffsetPx() > getSheetHeightForState(state)) return;

        // Updating the offset will automatically account for the browser controls.
        setSheetOffsetFromBottom(getCurrentOffsetPx(), StateChangeReason.SWIPE);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        // Trigger a relayout on window focus to correct any positioning issues when leaving Chrome
        // previously.  This is required as a layout is not triggered when coming back to Chrome
        // with the keyboard previously shown.
        if (hasWindowFocus) {
            ViewUtils.requestLayout(this, "BottomSheet.onWindowFocusChagned");
        }
    }

    @Override
    public boolean isContentScrolledToTop() {
        return mSheetContent == null || mSheetContent.getVerticalScrollOffset() <= 0;
    }

    @Override
    public float getCurrentOffsetPx() {
        return mCurrentOffsetPx;
    }

    @Override
    public float getMinOffsetPx() {
        return (swipeToDismissEnabled() ? getHiddenRatio() : getPeekRatio()) * mContainerHeight;
    }

    /**
     * Test whether a motion event is in the area of the sheet considered to be usable (i.e. not
     * on the shadow shown above the sheet or some other decorative part of the view).
     * @param event The motion event relative to the bottom sheet view.
     * @return Whether the event is considered to be in the usable area of the sheet.
     */
    public boolean isTouchEventInUsableArea(MotionEvent event) {
        return event.getY() > 0;
    }

    @Override
    public boolean isTouchEventInToolbar(MotionEvent event) {
        mToolbarHolder.getLocationOnScreen(mCachedLocation);

        // This check only tests for collision for the Y component since the sheet is the full width
        // of the screen. We only care if the touch event is above the bottom of the toolbar since
        // we won't receive an event if the touch is outside the sheet.
        return mCachedLocation[1] + mToolbarHolder.getHeight() > event.getRawY();
    }

    /** @return Whether flinging down hard enough will close the sheet. */
    private boolean swipeToDismissEnabled() {
        return mSheetContent != null ? mSheetContent.swipeToDismissEnabled() : true;
    }

    /** @return Whether the half state should be skipped when moving the sheet down. */
    private boolean shouldSkipHalfStateOnScrollingDown() {
        return mSheetContent == null || mSheetContent.skipHalfStateOnScrollingDown();
    }

    /**
     * @return The minimum sheet state that the user can swipe to. i.e. flinging down will either
     *         close the sheet or peek it.
     */
    @SheetState
    int getMinSwipableSheetState() {
        return swipeToDismissEnabled() || !isPeekStateEnabled()
                ? SheetState.HIDDEN
                : SheetState.PEEK;
    }

    /**
     * Get the state that the bottom sheet should open to with the provided content.
     * @return The minimum opened state for the current content.
     */
    @SheetState
    int getOpeningState() {
        if (mSheetContent == null) {
            return SheetState.HIDDEN;
        } else if (isPeekStateEnabled()) {
            return SheetState.PEEK;
        } else if (isHalfStateEnabled()) {
            return SheetState.HALF;
        }
        return SheetState.FULL;
    }

    @Override
    public float getMaxOffsetPx() {
        return getFullRatio() * mContainerHeight;
    }

    /**
     * Show content in the bottom sheet's content area.
     * @param content The {@link BottomSheetContent} to show, or null if no content should be shown.
     */
    void showContent(@Nullable final BottomSheetContent content) {
        // If the desired content is already showing, do nothing.
        if (mSheetContent == content) return;

        // Remove this as listener from previous content layout and size changes.
        if (mSheetContent != null) {
            mSheetContent.getContentView().removeOnLayoutChangeListener(this);
        }

        if (content != null && getParent() == null) {
            mSheetContainer.addView(this);
        } else if (content == null) {
            if (mSheetContainer.getParent() == null) {
                throw new RuntimeException(
                        "Attempting to detach sheet that was not in the hierarchy!");
            }
            mSheetContainer.removeView(this);
        }

        swapViews(
                content != null ? content.getContentView() : null,
                mSheetContent != null ? mSheetContent.getContentView() : null,
                mBottomSheetContentContainer);

        View newToolbar = content != null ? content.getToolbarView() : null;
        swapViews(
                newToolbar,
                mSheetContent != null ? mSheetContent.getToolbarView() : null,
                mToolbarHolder);

        onSheetContentChanged(content);
    }

    /**
     * Removes the oldView (or sets it to invisible) and adds the new view to the specified parent.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     */
    private void swapViews(final View newView, final View oldView, final ViewGroup parent) {
        if (oldView != null && oldView.getParent() != null) parent.removeView(oldView);
        if (newView != null && parent != newView.getParent()) parent.addView(newView);
    }

    /**
     * A notification that the sheet is exiting the peek state into one that shows content.
     * @param reason The reason the sheet was opened, if any.
     */
    private void onSheetOpened(@StateChangeReason int reason) {
        if (mIsSheetOpen) return;

        mIsSheetOpen = true;

        for (BottomSheetObserver o : mObservers) o.onSheetOpened(reason);
    }

    /**
     * A notification that the sheet has returned to the peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was closed,
     *         if any.
     */
    private void onSheetClosed(@StateChangeReason int reason) {
        if (!mIsSheetOpen) return;
        mIsSheetOpen = false;

        for (BottomSheetObserver o : mObservers) o.onSheetClosed(reason);
        // If the sheet contents are cleared out before #onSheetClosed is called, do not try to
        // retrieve the accessibility string.
        if (getCurrentSheetContent() != null) {
            announceForAccessibility(
                    getResources()
                            .getString(
                                    getCurrentSheetContent()
                                            .getSheetClosedAccessibilityStringId()));
        }
        clearFocus();

        setFocusable(false);
        setFocusableInTouchMode(false);
        setContentDescription(null);
    }

    /** Cancels and nulls the height animation if it exists. */
    private void cancelAnimation() {
        if (mSettleAnimator == null) return;
        mSettleAnimator.cancel();
        mSettleAnimator = null;
    }

    /**
     * Creates the sheet's animation to a target state.
     * @param targetState The target state.
     * @param reason The reason the sheet started animation.
     */
    private void createSettleAnimation(
            @SheetState final int targetState, @StateChangeReason final int reason) {
        mTargetState = targetState;
        mSettleAnimator =
                ValueAnimator.ofFloat(getCurrentOffsetPx(), getSheetHeightForState(targetState));
        boolean isExpand = targetState == SheetState.FULL;
        long duration = isExpand ? ANIMATION_DURATION_EXPAND_MS : ANIMATION_DURATION_SHRINK_MS;
        mSettleAnimator.setDuration(duration);
        mSettleAnimator.setInterpolator(Interpolators.EMPHASIZED);

        // When the animation is canceled or ends, reset the handle to null.
        mSettleAnimator.addListener(
                new CancelAwareAnimatorListener() {
                    @Override
                    public void onEnd(Animator animator) {
                        if (mIsDestroyed) return;

                        mSettleAnimator = null;
                        setInternalCurrentState(targetState, reason);
                        mTargetState = SheetState.NONE;
                    }
                });

        mSettleAnimator.addUpdateListener(
                new ValueAnimator.AnimatorUpdateListener() {
                    @Override
                    public void onAnimationUpdate(ValueAnimator animator) {
                        // Cancelled animation on M seem to continue updating, block them.
                        if (animator != mSettleAnimator) return;

                        setSheetOffsetFromBottom((Float) animator.getAnimatedValue(), reason);
                    }
                });

        setInternalCurrentState(SheetState.SCROLLING, reason);
        mSettleAnimator.start();
    }

    /** @return Get the height in px that the sheet is offset due to the browser controls. */
    private float getOffsetFromBrowserControls() {
        if (mSheetContent == null || !mSheetContent.hideOnScroll()) return 0;

        // We only care about peek/half state.
        int state = getSheetState();

        // Returns non-zero offset for the opening animation. This keeps the animation running
        // below the bottom of the screen.
        if (mAlwaysFullWidth
                && state == SheetState.SCROLLING
                && mTargetState == SheetState.PEEK
                && mBrowserControlsHiddenRatio == 1.f) {
            state = mTargetState;
        }
        if (state != SheetState.PEEK && state != SheetState.HALF) return 0;
        return getSheetHeightForState(state) * mBrowserControlsHiddenRatio;
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     * @param reason The reason for the sheet offset to change to report to listeners.
     */
    void setSheetOffsetFromBottom(float offset, @StateChangeReason int reason) {
        setSheetOffsetFromBottom(offset, reason, /* reportOpenClosed= */ true);
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     * @param reason The reason for the sheet offset to change to report to listeners.
     * @param reportOpenClosed {@code true} to allow reporting the sheet opened or closed as a
     *         result of this change. {@code reason} is never used when this is {@code false}.
     */
    void setSheetOffsetFromBottom(
            float offset, @StateChangeReason int reason, boolean reportOpenClosed) {
        mCurrentOffsetPx = offset;

        assert mEdgeToEdgeBottomInsetSupplier.get() != null;
        int bottomInset = ViewUtils.dpToPx(getContext(), mEdgeToEdgeBottomInsetSupplier.get());

        // The browser controls offset is added here so that the sheet's toolbar behaves like the
        // browser controls do.
        float translationY =
                (mContainerHeight - mCurrentOffsetPx)
                        + getOffsetFromBrowserControls()
                        - (mTargetState == SheetState.HIDDEN ? 0 : bottomInset);

        if (isSheetOpen() && MathUtils.areFloatsEqual(translationY, getTranslationY())) return;

        setTranslationY(translationY);

        if (reportOpenClosed) {
            // Do open/close computation based on the minimum allowed state by the sheet's content.
            // Note that when transitioning from hidden to peek, even dismissable sheets may want
            // to have a peek state.
            @SheetState int minSwipableState = getMinSwipableSheetState();
            if (isPeekStateEnabled() && (!isSheetOpen() || mTargetState == SheetState.PEEK)) {
                minSwipableState = SheetState.PEEK;
            }

            float minScrollableHeight = getSheetHeightForState(minSwipableState);
            boolean isAtMinHeight =
                    MathUtils.areFloatsEqual(getCurrentOffsetPx(), minScrollableHeight);
            boolean heightLessThanPeek = getCurrentOffsetPx() < minScrollableHeight;

            if (isSheetOpen() && (heightLessThanPeek || isAtMinHeight)) {
                onSheetClosed(reason);
            } else if (!isSheetOpen()
                    && mTargetState != SheetState.HIDDEN
                    && getCurrentOffsetPx() > minScrollableHeight) {
                onSheetOpened(reason);
            }
        }

        sendOffsetChangeEvents();
    }

    @Override
    public void setSheetOffset(float offset, boolean shouldAnimate) {
        cancelAnimation();
        if (mSheetContent == null) return;

        if (shouldAnimate) {
            float velocityY = getCurrentOffsetPx() - offset;

            @SheetState int targetState = getTargetSheetState(offset, -velocityY);

            setSheetState(targetState, true, StateChangeReason.SWIPE);
        } else {
            setInternalCurrentState(SheetState.SCROLLING, StateChangeReason.SWIPE);
            setSheetOffsetFromBottom(offset, StateChangeReason.SWIPE);
        }
    }

    /** @return The ratio of the height of the screen that the hidden state is. */
    @VisibleForTesting
    float getHiddenRatio() {
        return 0;
    }

    /** @return Whether the peeking state for the sheet's content is enabled. */
    boolean isPeekStateEnabled() {
        return mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DISABLED;
    }

    /** @return Whether the half-height of the sheet is enabled. */
    private boolean isHalfStateEnabled() {
        if (mSheetContent == null) return false;

        // Half state is invalid on small screens, when wrapping content at full height, and when
        // explicitly disabled.
        return !isSmallScreen()
                && mSheetContent.getHalfHeightRatio() != HeightMode.DISABLED
                && mSheetContent.getFullHeightRatio() != HeightMode.WRAP_CONTENT;
    }

    /** @return Whether the height mode for the full state is WRAP_CONTENT. */
    private boolean isFullHeightWrapContent() {
        return mSheetContent != null
                && mSheetContent.getFullHeightRatio() == HeightMode.WRAP_CONTENT;
    }

    /** @return The ratio of the height of the screen that the peeking state is. */
    public float getPeekRatio() {
        if (mContainerHeight <= 0 || !isPeekStateEnabled()) return 0;

        // If the content has a custom peek ratio set, use that instead of computing one.
        if (mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DEFAULT) {
            assert mSheetContent.getPeekHeight() != HeightMode.WRAP_CONTENT
                    : "The peek mode can't wrap content.";
            float ratio = mSheetContent.getPeekHeight() / (float) mContainerHeight;
            assert ratio > 0 && ratio <= 1 : "Custom peek ratios must be in the range of (0, 1].";
            return ratio;
        }

        View toolbarView = getToolbarView();

        int toolbarHeight;
        if (toolbarView == null) {
            toolbarHeight = getResources().getDimensionPixelSize(R.dimen.bottom_sheet_peek_height);
        } else {
            toolbarHeight = toolbarView.getHeight();
            if (toolbarHeight == 0) {
                // If the toolbar is not laid out yet and has a fixed height layout parameter, we
                // assume that the toolbar will have this height in the future.
                ViewGroup.LayoutParams layoutParams = toolbarView.getLayoutParams();
                if (layoutParams != null) {
                    if (layoutParams.height > 0) {
                        toolbarHeight = layoutParams.height;
                    } else {
                        toolbarView.measure(
                                MeasureSpec.makeMeasureSpec(
                                        getMaxSheetWidth(), MeasureSpec.EXACTLY),
                                MeasureSpec.makeMeasureSpec(
                                        getMaxContentHeight(), MeasureSpec.AT_MOST));
                        toolbarHeight = toolbarView.getMeasuredHeight();
                    }
                }
            }
        }
        return toolbarHeight / (float) mContainerHeight;
    }

    private View getToolbarView() {
        return mSheetContent != null && mSheetContent.getToolbarView() != null
                ? mSheetContent.getToolbarView()
                : null;
    }

    /** @return The ratio of the height of the screen that the half expanded state is. */
    @VisibleForTesting
    float getHalfRatio() {
        if (mContainerHeight <= 0 || !isHalfStateEnabled()) return 0;

        float customHalfRatio = mSheetContent.getHalfHeightRatio();
        assert customHalfRatio != HeightMode.WRAP_CONTENT
                : "Half-height cannot be WRAP_CONTENT. This is only supported for full-height.";

        return customHalfRatio == HeightMode.DEFAULT ? HALF_HEIGHT_RATIO : customHalfRatio;
    }

    /** @return The ratio of the height of the screen that the fully expanded state is. */
    @VisibleForTesting
    float getFullRatio() {
        if (mContainerHeight <= 0 || mSheetContent == null) return 0;

        float customFullRatio = mSheetContent.getFullHeightRatio();
        assert customFullRatio != HeightMode.DISABLED : "The full height cannot be DISABLED.";

        if (isFullHeightWrapContent()) {
            ensureContentDesiredHeightIsComputed();
            return Math.min(getMaxContentHeight(), mContentDesiredHeight) / mContainerHeight;
        }

        return customFullRatio == HeightMode.DEFAULT ? 1 : customFullRatio;
    }

    /** @return The height of the container that the bottom sheet exists in. */
    public float getSheetContainerHeight() {
        return mContainerHeight;
    }

    /**
     * Sends notifications if the sheet is transitioning from the peeking to half expanded state and
     * from the peeking to fully expanded state. The peek to half events are only sent when the
     * sheet is between the peeking and half states.
     */
    private void sendOffsetChangeEvents() {
        float offsetWithBrowserControls = getCurrentOffsetPx() - getOffsetFromBrowserControls();

        // Do not send events for states less than the hidden state unless 0 has not been sent.
        if (offsetWithBrowserControls <= getSheetHeightForState(SheetState.HIDDEN)
                && mLastOffsetRatioSent <= 0) {
            return;
        }

        float screenRatio =
                mContainerHeight > 0 ? offsetWithBrowserControls / (float) mContainerHeight : 0;

        // This ratio is relative to the peek and full positions of the sheet.
        float maxHiddenFullRatio = getFullRatio() - getHiddenRatio();
        float hiddenFullRatio =
                maxHiddenFullRatio == 0
                        ? 0
                        : MathUtils.clamp(
                                (screenRatio - getHiddenRatio()) / maxHiddenFullRatio, 0, 1);

        if (offsetWithBrowserControls < getSheetHeightForState(SheetState.HIDDEN)) {
            mLastOffsetRatioSent = 0;
        } else {
            mLastOffsetRatioSent =
                    MathUtils.areFloatsEqual(hiddenFullRatio, 0) ? 0 : hiddenFullRatio;
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetOffsetChanged(mLastOffsetRatioSent, getCurrentOffsetPx());
        }
    }

    /** @see #setSheetState(int, boolean, int) */
    void setSheetState(@SheetState int state, boolean animate) {
        setSheetState(state, animate, StateChangeReason.NONE);
    }

    /**
     * Moves the sheet to the provided state.
     * @param state The state to move the panel to. This cannot be SheetState.SCROLLING or
     *              SheetState.NONE.
     * @param animate If true, the sheet will animate to the provided state, otherwise it will
     *                move there instantly.
     * @param reason The reason the sheet state is changing. This can be specified to indicate to
     *               observers that a more specific event has occurred, otherwise
     *               STATE_CHANGE_REASON_NONE can be used.
     */
    void setSheetState(@SheetState int state, boolean animate, @StateChangeReason int reason) {
        assert state != SheetState.NONE;

        // Setting state to SCROLLING is not a valid operation. This can happen only when
        // we're already in the scrolling state. Make it no-op.
        if (state == SheetState.SCROLLING) {
            // TODO(mdjones): The isRunningSettleAnimation should hold but currently doesn't.
            assert mCurrentState == SheetState.SCROLLING; // && isRunningSettleAnimation();
            return;
        }

        if (state == SheetState.HALF && !isHalfStateEnabled()) state = SheetState.FULL;

        cancelAnimation();
        mTargetState = state;

        if (animate
                && (state != mCurrentState
                        || mCurrentOffsetPx != getSheetHeightForState(mTargetState))) {
            createSettleAnimation(state, reason);
        } else {
            setSheetOffsetFromBottom(getSheetHeightForState(state), reason);
            setInternalCurrentState(mTargetState, reason);
            mTargetState = SheetState.NONE;
        }
    }

    /**
     * @return The target state that the sheet is moving to during animation. If the sheet is
     *         stationary or a target state has not been determined, SheetState.NONE will be
     *         returned.
     */
    int getTargetSheetState() {
        return mTargetState;
    }

    /**
     * @return The current state of the bottom sheet. If the sheet is animating, this will be the
     *         state the sheet is animating to.
     */
    @SheetState
    int getSheetState() {
        return mCurrentState;
    }

    /** @return Whether the sheet is currently open. */
    boolean isSheetOpen() {
        return mIsSheetOpen;
    }

    /**
     * Set the current state of the bottom sheet. This is for internal use to notify observers of
     * state change events.
     * @param state The current state of the sheet.
     * @param reason The reason the state is changing if any.
     */
    private void setInternalCurrentState(@SheetState int state, @StateChangeReason int reason) {
        if (state == mCurrentState) return;

        // If we somehow got here with null content, force the sheet to close without animation.
        // See https://crbug.com/1126872 for more information.
        if (getCurrentSheetContent() == null && state != SheetState.HIDDEN) {
            Throwable throwable =
                    new Throwable(
                            "This is not a crash. See https://crbug.com/1126872 for details.");
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> sExceptionReporter.onResult(throwable));

            setSheetState(SheetState.HIDDEN, false);
            return;
        }

        // TODO(mdjones): This shouldn't be able to happen, but does occasionally during layout.
        //                Fix the race condition that is making this happen.
        if (state == SheetState.NONE) {
            setSheetState(getTargetSheetState(getCurrentOffsetPx(), 0), false);
            return;
        }

        // Remember which state precedes the scrolling.
        mScrollingStartState =
                state == SheetState.SCROLLING
                        ? mCurrentState != SheetState.SCROLLING ? mCurrentState : SheetState.NONE
                        : SheetState.NONE; // Not scrolling anymore.
        mCurrentState = state;

        if (mCurrentState == SheetState.HALF || mCurrentState == SheetState.FULL) {
            int resId =
                    mCurrentState == SheetState.FULL
                            ? getCurrentSheetContent().getSheetFullHeightAccessibilityStringId()
                            : getCurrentSheetContent().getSheetHalfHeightAccessibilityStringId();
            announceForAccessibility(getResources().getString(resId));

            // TalkBack will announce the content description if it has changed, so wait to set the
            // content description until after announcing full/half height.
            setFocusable(true);
            setFocusableInTouchMode(true);
            String contentDescription =
                    getResources()
                            .getString(
                                    getCurrentSheetContent().getSheetContentDescriptionStringId());

            if (getCurrentSheetContent().swipeToDismissEnabled()) {
                contentDescription +=
                        ". "
                                + getResources()
                                        .getString(R.string.bottom_sheet_accessibility_description);
            }

            setContentDescription(contentDescription);
            if (getFocusedChild() == null) requestFocus();
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetStateChanged(mCurrentState, reason);
        }
    }

    /**
     * If the animation to settle the sheet in one of its states is running.
     * @return True if the animation is running.
     */
    private boolean isRunningSettleAnimation() {
        return mSettleAnimator != null;
    }

    /** @return The current sheet content, or null if there is no content. */
    @Nullable
    BottomSheetContent getCurrentSheetContent() {
        return mSheetContent;
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    private float getSheetHeightForState(@SheetState int state) {
        if (isFullHeightWrapContent() && state == SheetState.FULL) {
            ensureContentDesiredHeightIsComputed();
        }

        return getRatioForState(state) * mContainerHeight;
    }

    /** @return The max possible height that the content can be. */
    private int getMaxContentHeight() {
        return mContainerHeight;
    }

    /** @return The maximum width of the bottom sheet based on its current state and container. */
    private int getMaxSheetWidth() {
        if (!mAlwaysFullWidth) {
            int narrowWidthThreshold =
                    getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width_threshold);
            if (mContainerWidth > narrowWidthThreshold) {
                return getResources().getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width);
            }
        }
        return mContainerWidth;
    }

    /**
     * @return Whether the sheet covers the full width of the container, or is limited to only
     *     partial width.
     */
    public boolean isFullWidth() {
        return getMaxSheetWidth() >= mContainerWidth;
    }

    /** Center and size the sheet in its container. */
    private void sizeAndPositionSheetInParent() {
        int maxSheetWidth = getMaxSheetWidth();
        getLayoutParams().width = maxSheetWidth;
        setTranslationX(
                (LocalizationUtils.isLayoutRtl() ? -1 : 1)
                        * (mContainerWidth - maxSheetWidth)
                        / 2f);
        ViewUtils.requestLayout(this, "BottomSheet.sizeAndPositionSheetInParent");
    }

    private void ensureContentDesiredHeightIsComputed() {
        if (mContentDesiredHeight != HEIGHT_UNSPECIFIED) {
            return;
        }
        mSheetContent
                .getContentView()
                .measure(
                        MeasureSpec.makeMeasureSpec(getMaxSheetWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(getMaxContentHeight(), MeasureSpec.AT_MOST));
        mContentDesiredHeight = mSheetContent.getContentView().getMeasuredHeight();
    }

    private float getRatioForState(int state) {
        switch (state) {
            case SheetState.HIDDEN:
                return getHiddenRatio();
            case SheetState.PEEK:
                return getPeekRatio();
            case SheetState.HALF:
                return getHalfRatio();
            case SheetState.FULL:
                return getFullRatio();
        }

        throw new IllegalArgumentException("Invalid state: " + state);
    }

    /**
     * Adds an observer to the bottom sheet.
     * @param observer The observer to add.
     */
    void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to the bottom sheet.
     * @param observer The observer to remove.
     */
    void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets the target state of the sheet based on the sheet's height and velocity.
     * @param sheetHeight The current height of the sheet.
     * @param yVelocity The current Y velocity of the sheet. If this value is positive, the movement
     *                  is from bottom to top.
     * @return The target state of the bottom sheet.
     */
    @SheetState
    private int getTargetSheetState(float sheetHeight, float yVelocity) {
        if (sheetHeight <= getMinOffsetPx()) return getMinSwipableSheetState();
        if (sheetHeight >= getMaxOffsetPx()) return SheetState.FULL;

        boolean isMovingDownward = yVelocity < 0;

        // If velocity shouldn't affect dismissing the sheet, reverse effect on the sheet height.
        if (isMovingDownward && !swipeToDismissEnabled()) sheetHeight -= yVelocity;

        // Find the two states that the sheet height is between.
        @SheetState int prevState = mScrollingStartState;
        @SheetState
        int nextState =
                isMovingDownward
                        ? getLargestCollapsingState(isMovingDownward, sheetHeight)
                        : getSmallestExpandingState(isMovingDownward, sheetHeight);

        // Go into the next state only if the threshold for minimal change has been cleared.
        return hasCrossedThresholdToNextState(prevState, nextState, sheetHeight, isMovingDownward)
                ? nextState
                : prevState;
    }

    /**
     * Returns whether the sheet was scrolled far enough to transition into the next state.
     * @param prev The state before the scrolling transition happened.
     * @param next The state before the scrolling transitions into.
     * @param sheetMovesDown True if the sheet moves down.
     * @param sheetHeight The current sheet height in flux.
     * @return True, iff the sheet was scrolled far enough to transition from |prev| to |next|.
     */
    private boolean hasCrossedThresholdToNextState(
            @SheetState int prev, @SheetState int next, float sheetHeight, boolean sheetMovesDown) {
        if (next == prev) return false;
        // Moving from an internal/temporary state always works:
        if (prev == SheetState.NONE || prev == SheetState.SCROLLING) return true;
        float lowerBound = getSheetHeightForState(prev);
        float distance = getSheetHeightForState(next) - lowerBound;
        return Math.abs((sheetHeight - lowerBound) / distance)
                > getThresholdToNextState(prev, next, sheetMovesDown);
    }

    /**
     * The threshold to enter a state depends on whether a transition skips the half state. The more
     * states to cross, the smaller the (percentual) threshold. A small threshold is used iff:
     *   * It doesn't move into the HALF state,
     *   * Skipping the HALF state is allowed, and
     *   * The is large enough to skip the HALF state
     * @param prev The state before the scrolling transition happened.
     * @param next The state before the scrolling transitions into.
     * @param sheetMovesDown True if the sheet is being moved down.
     * @return a threshold (as percentage of the scroll distance covered).
     */
    private float getThresholdToNextState(
            @SheetState int prev, @SheetState int next, boolean sheetMovesDown) {
        if (next == SheetState.HALF) return THRESHOLD_TO_NEXT_STATE_3;
        boolean crossesHalf =
                (sheetMovesDown && prev > SheetState.HALF && next < SheetState.HALF)
                        || (!sheetMovesDown && prev < SheetState.HALF && next > SheetState.HALF);
        if (!crossesHalf) return THRESHOLD_TO_NEXT_STATE_3;
        if (!shouldSkipHalfStateOnScrollingDown()) return THRESHOLD_TO_NEXT_STATE_3;
        return THRESHOLD_TO_NEXT_STATE_2;
    }

    /**
     * Returns the largest, acceptable state whose height is smaller than the given sheet height.
     * E.g. if a sheet is between FULL and HALF, collapsing states are PEEK and HALF. Although HALF
     * is closer to the sheet's height, it might have to be skipped. Then, PEEK is returned instead.
     * @param sheetMovesDown If the sheet moves down, some smaller states might be skipped.
     * @param sheetHeight The current sheet height in flux.
     * @return The largest, acceptable, collapsing state.
     */
    private @SheetState int getLargestCollapsingState(boolean sheetMovesDown, float sheetHeight) {
        @SheetState int largestCollapsingState = getMinSwipableSheetState();
        boolean skipHalfState = !isHalfStateEnabled() || shouldSkipHalfStateOnScrollingDown();
        for (@SheetState int i = largestCollapsingState + 1; i < SheetState.FULL; i++) {
            if (i == SheetState.PEEK && !isPeekStateEnabled()) continue;
            if (i == SheetState.HALF && skipHalfState) continue;

            if (sheetHeight > getSheetHeightForState(i)
                    || (sheetHeight == getSheetHeightForState(i) && !sheetMovesDown)) {
                largestCollapsingState = i;
            }
        }
        return largestCollapsingState;
    }

    /**
     * Returns the smallest, acceptable state whose height is larger than the given sheet height.
     * E.g. if the sheet is between PEEK and HALF, expanding states are HALF and FULL. Although HALF
     * is closer to the sheet's height, it might not be enabled. Then, FULL is returned instead.
     * @param sheetMovesDown If the sheet moves down, some collapsing states might be skipped. This
     *                       affects the smallest possible expanding state as well.
     * @param sheetHeight The current sheet height in flux.
     * @return The smallest, acceptable, expanding state.
     */
    private @SheetState int getSmallestExpandingState(boolean sheetMovesDown, float sheetHeight) {
        @SheetState
        int largestCollapsingState = getLargestCollapsingState(sheetMovesDown, sheetHeight);
        @SheetState int smallestExpandingState = SheetState.FULL;
        for (@SheetState int i = smallestExpandingState - 1; i > largestCollapsingState + 1; i--) {
            if (i == SheetState.HALF && !isHalfStateEnabled()) continue;
            if (i == SheetState.PEEK && !isPeekStateEnabled()) continue;

            if (sheetHeight <= getSheetHeightForState(i)) {
                smallestExpandingState = i;
            }
        }

        return smallestExpandingState;
    }

    public static void setSmallScreenForTesting(boolean isSmallScreen) {
        sIsSmallScreenForTesting = isSmallScreen;
        ResettersForTesting.register(() -> sIsSmallScreenForTesting = null);
    }

    public boolean isSmallScreen() {
        if (sIsSmallScreenForTesting != null) return sIsSmallScreenForTesting;

        // A small screen is defined by there being less than 160dp between half and full states.
        float fullToHalfDiff = (1 - HALF_HEIGHT_RATIO) * mContainerHeight;
        return fullToHalfDiff < mMinHalfFullDistance;
    }

    /**
     * Called when the sheet content has changed, to update dependent state and notify observers.
     *
     * @param content The new sheet content, or null if the sheet has no content.
     */
    protected void onSheetContentChanged(@Nullable final BottomSheetContent content) {
        mSheetContent = content;

        boolean shouldLongPressMoveSheet =
                content == null ? false : content.shouldLongPressMoveSheet();
        mGestureDetector.setShouldLongPressMoveSheet(shouldLongPressMoveSheet);

        if (content != null && isFullHeightWrapContent()) {
            // Listen for layout/size changes.
            content.getContentView().addOnLayoutChangeListener(this);

            invalidateContentDesiredHeight();
            ensureContentIsWrapped(/* animate= */ true);

            // HALF state is forbidden when wrapping the content.
            if (mCurrentState == SheetState.HALF) {
                setSheetState(SheetState.FULL, /* animate= */ true);
            }
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
        mToolbarHolder.setBackgroundColor(Color.TRANSPARENT);
    }

    /** Called when the sheet content layout changed. */
    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // When there is a device rotation, mContentWidth needs to be updated before the new
        // view is drawn.
        invalidateContentDesiredHeight();
        ensureContentIsWrapped(/* animate= */ true);

        // If the sheet height changes mid-animation, make sure we animate to that height.
        // TODO(330357665): This animation will look rough in most cases, we should investigate a
        //                  way to smooth this.
        int newHeight = bottom - top;
        int oldHeight = oldBottom - oldTop;
        if (isRunningSettleAnimation() && isFullHeightWrapContent() && oldHeight != newHeight) {
            @SheetState int target = getTargetSheetState();
            if (target != SheetState.NONE) {
                cancelAnimation();
                setSheetState(target, /* animate= */ true);
            }
        }
    }

    /**
     * Updates the sheet container's top margin to avoid drawing the sheet into the app header.
     *
     * @param appHeaderHeight The app header height.
     */
    void onAppHeaderHeightChanged(int appHeaderHeight) {
        assert mSheetContainer != null : "Sheet container should not be null.";
        var params = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        if (params.topMargin != mAppHeaderHeight) {
            // Log to track cases where the top margin was updated by some other source.
            Log.i(
                    TAG,
                    "Current top margin="
                            + params.topMargin
                            + ", previous app header height="
                            + mAppHeaderHeight
                            + ", new app header height="
                            + appHeaderHeight);
        }
        mAppHeaderHeight = appHeaderHeight;
        if (appHeaderHeight != params.topMargin) {
            params.topMargin = appHeaderHeight;
            mSheetContainer.setLayoutParams(params);
        }
    }

    private void ensureContentIsWrapped(boolean animate) {
        if (mCurrentState == SheetState.HIDDEN || mCurrentState == SheetState.PEEK) return;

        // The SCROLLING state is used when animating the sheet height or when the user is swiping
        // the sheet. If it is the latter, we should not change the sheet height.
        if (!isRunningSettleAnimation() && mCurrentState == SheetState.SCROLLING) return;
        setSheetState(mCurrentState, animate);
    }

    private void invalidateContentDesiredHeight() {
        mContentDesiredHeight = HEIGHT_UNSPECIFIED;
    }

    /**
     * WARNING: This destroys the state of the BottomSheet. Only use in tests and only use once.
     * Puts the sheet into a scrolling state that can't be reached in tests otherwise.
     *
     * @param sheetHeightInPx The height in px that the sheet should be "scrolled" to.
     * @param yUpwardsVelocity The sheet's upwards y velocity when reaching the scrolled height.
     * @return The state the bottom sheet would target when the scrolling ends.
     */
    @VisibleForTesting
    @SheetState
    int forceScrollingStateForTesting(float sheetHeightInPx, float yUpwardsVelocity) {
        mScrollingStartState = mCurrentState;
        mCurrentState = SheetState.SCROLLING;
        return getTargetSheetState(sheetHeightInPx, yUpwardsVelocity);
    }

    void setSheetContainerForTesting(ViewGroup sheetContainer) {
        mSheetContainer = sheetContainer;
    }
}
