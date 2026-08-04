// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.TokenHolder;

import java.util.List;
import java.util.function.Supplier;

/**
 * This class defines the bottom sheet that has multiple states and a persistently showing toolbar.
 * Namely, the states are: - PEEK: Only the toolbar is visible at the bottom of the screen. - HALF:
 * The sheet is expanded to consume around half of the screen. - FULL: The sheet is expanded to its
 * full height.
 *
 * <p>All the computation in this file is based off of the bottom of the screen instead of the top
 * for simplicity. This means that the bottom of the screen is 0 on the Y axis.
 */
@NullMarked
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

    private static final GlowSpec DEFAULT_GLOW_SPEC = new GlowSpec(0, GlowSpec.ShadowSize.DEFAULT);

    /** The height ratio for the sheet in the SheetState.HALF state. */
    private static final float HALF_HEIGHT_RATIO = 0.75f;

    /** The maximum height ratio for the sheet. */
    private static final float MAX_HEIGHT_RATIO = 1.0f;

    /** The desired height of a content that has just been shown or whose height was invalidated. */
    private static final float HEIGHT_UNSPECIFIED = -1.0f;

    /** A means of reporting an exception/stack without crashing. */
    private static @Nullable Callback<Throwable> sExceptionReporter;

    /** A flag to force the small screen state of the bottom sheet. */
    private static @Nullable Boolean sIsSmallScreenForTesting;

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

    /**
     * The view that is used to the area below the bottom sheet contents that is normally obscured
     * by the keyboard.
     */
    private View mKeyboardCurtain;

    /** The view that contains the sheet background color. */
    private View mSheetBackground;

    /** TokenHolder for tracking keyboard visibility. */
    private final TokenHolder mKeyboardTokenHolder = new TokenHolder(CallbackUtils.emptyRunnable());

    /** The token for the keyboard visibility. */
    private int mKeyboardToken = TokenHolder.INVALID_TOKEN;

    /** The state of the sheet before the keyboard was shown. */
    private @SheetState int mStateBeforeKeyboardShown = SheetState.NONE;

    /** The height of the screen in the previous layout pass. */
    private int mPreviousScreenHeight;

    /** The view that contains the sheet background glow color. */
    private View mShadowLayer;

    /**
     * An alternative shadow layer used exclusively on large form factor devices when the current
     * sheet content opts out of the new bottom sheet UI. This provides the standard mobile
     * bottom-edge bleeder shadow instead of the full perimeter rectangle shadow.
     */
    private @Nullable View mFallbackShadowLayer;

    /** For detecting scroll and fling events on the bottom sheet. */
    private final BottomSheetSwipeDetector mGestureDetector;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private @Nullable ValueAnimator mSettleAnimator;

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
    protected @Nullable BottomSheetContent mSheetContent;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /**
     * The optional 'X' close button. This is injected into large form factor layouts when the sheet
     * is non-modal (meaning it lacks a background scrim that would otherwise allow the user to
     * easily tap-to-dismiss).
     */
    private View mCloseButton;

    /** A handle to the FrameLayout that holds the snackbar of the bottom sheet. */
    private @Nullable FrameLayout mSnackbarContainer;

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

    /** Whether {@link #destroy()} has been called. */
    private boolean mIsDestroyed;

    /** The ratio in the range [0, 1] that the browser controls are hidden. */
    private float mBrowserControlsHiddenRatio;

    /** Whether or not always use the full width of the container. */
    private boolean mAlwaysFullWidth;

    /** Whether the device is on a platform that supports a large form factor. */
    private boolean mIsLargeFormFactor;

    /** The window for the bottom sheet. */
    private @MonotonicNonNull Window mWindow;

    /** The supplier of the bottom inset when edge to edge is enabled. */
    private Supplier<Integer> mEdgeToEdgeBottomInsetSupplier = () -> 0;

    /** Observer for inset changes. */
    private InsetObserver mInsetObserver;

    /** The last recorded app header height, in px. */
    private int mAppHeaderHeight;

    private int mBottomMargin;
    private @ColorInt int mSheetBgColor;

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
            setShadowLength(
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length));
        }

        public void setShadowLength(int length) {
            mShadowLength = length;
            setTranslationX((LocalizationUtils.isLayoutRtl() ? 1 : -1) * mShadowLength);
            setTranslationY(-mShadowLength);
            requestLayout();
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
        float endX = mContainerWidth + mVisibleViewportRect.left;
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
        mSheetBgColor = getNonModalBottomSheetBgColor(context);
        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
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
     * @param bottomMargin The extra margin to add to the bottom of sheet container.
     * @param insetObserver An observer for inset changes.
     */
    @Initializer
    public void init(
            Window window,
            KeyboardVisibilityDelegate keyboardDelegate,
            boolean alwaysFullWidth,
            Supplier<Integer> edgeToEdgeBottomInsetSupplier,
            int appHeaderHeight,
            int bottomMargin,
            InsetObserver insetObserver,
            boolean isLargeFormFactor) {
        mWindow = window;
        mEdgeToEdgeBottomInsetSupplier = edgeToEdgeBottomInsetSupplier;
        mInsetObserver = insetObserver;
        mSheetContainer = (ViewGroup) getParent();
        mKeyboardCurtain = findViewById(R.id.keyboard_curtain);
        mSheetBackground = findViewById(R.id.background);
        mShadowLayer = findViewById(R.id.shadow_layer);
        mFallbackShadowLayer = findViewById(R.id.desktop_fallback_shadow);
        onAppHeaderHeightChanged(appHeaderHeight);
        setBottomMargin(bottomMargin);

        mToolbarHolder = findViewById(R.id.bottom_sheet_toolbar_container);
        mToolbarHolder.setBottomSheet(this);

        mBottomSheetContentContainer = findViewById(R.id.bottom_sheet_content);
        mBottomSheetContentContainer.setBottomSheet(this);

        mCloseButton = findViewById(R.id.bottom_sheet_close_button);

        mSnackbarContainer = findViewById(R.id.bottom_sheet_snackbar_container);
        assert mSnackbarContainer != null;

        mContainerWidth = mSheetContainer.getWidth();
        mContainerHeight = mSheetContainer.getHeight();
        mAlwaysFullWidth = alwaysFullWidth;
        mIsLargeFormFactor = isLargeFormFactor;

        sizeAndPositionSheetInParent();

        // Listen to height changes on the root.
        mSheetContainer.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    private int mPreviousViewportBottomInset;

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

                            for (BottomSheetObserver obs : mObservers) {
                                obs.onContainerSizeChanged(mContainerWidth, mContainerHeight);
                            }
                        }

                        updateContentContainerHeight();

                        @Px int viewportBottomInset = getViewportBottomInset();
                        if (previousHeight != mContainerHeight
                                || mPreviousViewportBottomInset != viewportBottomInset) {
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

                        maybeRevertStateOnLayoutChange();
                        mPreviousViewportBottomInset = viewportBottomInset;
                    }
                });

        mInsetObserver.addWindowInsetsAnimationListener(
                new WindowInsetsAnimationListener() {
                    @Override
                    public void onPrepare(WindowInsetsAnimationCompat animation) {
                        for (BottomSheetObserver obs : mObservers) {
                            obs.beforeInsetAnimationStart();
                        }
                    }

                    @Override
                    public void onStart(
                            WindowInsetsAnimationCompat animation,
                            WindowInsetsAnimationCompat.BoundsCompat bounds) {
                        onInsetChanged();
                    }

                    @Override
                    public void onProgress(
                            WindowInsetsCompat insets, List<WindowInsetsAnimationCompat> list) {
                        onInsetChanged();
                    }

                    @Override
                    public void onEnd(WindowInsetsAnimationCompat animation) {
                        onInsetChanged();
                        for (BottomSheetObserver obs : mObservers) {
                            obs.onInsetAnimationEnd();
                        }
                    }
                });

        // Listen to height changes on the toolbar.
        mToolbarHolder.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    // Make sure the size of the layout actually changed.
                    if (bottom - top == oldBottom - oldTop && right - left == oldRight - oldLeft) {
                        return;
                    }

                    if (!mGestureDetector.isScrolling() && isRunningSettleAnimation()) return;

                    setSheetState(mCurrentState, false);
                });

        mSheetContainer.removeView(this);
    }

    private void onInsetChanged() {
        maybeCacheStateOnKeyboardShown();
        updateContentContainerHeight();
    }

    private int getEdgeToEdgeBottomInset() {
        return mBottomMargin == 0
                ? ViewUtils.dpToPx(getContext(), mEdgeToEdgeBottomInsetSupplier.get())
                : 0;
    }

    private int getViewportBottomInset() {
        assert mEdgeToEdgeBottomInsetSupplier.get() != null;
        @Px int viewportBottomInset = getEdgeToEdgeBottomInset();

        if (isSheetOpen()) {
            int visibleViewport = mVisibleViewportRect.height();
            viewportBottomInset = Math.max(viewportBottomInset, mContainerHeight - visibleViewport);
        }
        return viewportBottomInset;
    }

    /**
     * Detects keyboard being closed upon layout changes. Done lazily during layout changes instead
     * of directly in a keyboard visibility listener due since layout changes will not have been
     * processed yet in the bottom sheet.
     */
    private void maybeRevertStateOnLayoutChange() {
        assert mWindow != null;

        // If the screen height has changed, reset the cached state since it may no longer valid.
        @Px int decorHeight = mWindow.getDecorView().getHeight();
        if (mPreviousScreenHeight != decorHeight) {
            resetCachedKeyboardState();
        }

        boolean keyboardVisible = isKeyboardShowing();
        if (!keyboardVisible
                && mKeyboardToken != TokenHolder.INVALID_TOKEN
                && mStateBeforeKeyboardShown != SheetState.NONE
                && isFullHeightResizeContent()) {
            assert mKeyboardTokenHolder.hasTokens();
            setInternalCurrentState(SheetState.NONE, StateChangeReason.NONE);
            setSheetState(mStateBeforeKeyboardShown, /* animate= */ false);

            resetCachedKeyboardState();
        }


        mPreviousScreenHeight = decorHeight;
    }

    private boolean isKeyboardShowing() {
        return mInsetObserver.getSupplierForKeyboardInset().get() > 0;
    }

    private void maybeCacheStateOnKeyboardShown() {
        if (isKeyboardShowing() && mStateBeforeKeyboardShown == SheetState.NONE) {
            assert mKeyboardToken == TokenHolder.INVALID_TOKEN;
            assert !mKeyboardTokenHolder.hasTokens();

            // The bottom sheet state will not have been updated yet at this point, so
            // store for later use.
            mStateBeforeKeyboardShown = mCurrentState;
            mKeyboardToken = mKeyboardTokenHolder.acquireToken();
        }
    }

    /**
     * @param ratio The current browser controls hidden ratio.
     */
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
        return getFullRatio() * getMaxSheetHeight();
    }

    /**
     * Show content in the bottom sheet's content area.
     *
     * @param content The {@link BottomSheetContent} to show, or null if no content should be shown.
     */
    void showContent(final @Nullable BottomSheetContent content) {
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
    private void swapViews(
            final @Nullable View newView, final @Nullable View oldView, final ViewGroup parent) {
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
        setFocusable(true);
        setFocusableInTouchMode(true);
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
     *
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
                animator -> {
                    // Cancelled animation on M seem to continue updating, block them.
                    if (animator != mSettleAnimator) return;

                    setSheetOffsetFromBottom((Float) animator.getAnimatedValue(), reason);
                });

        setInternalCurrentState(SheetState.SCROLLING, reason);
        mSettleAnimator.start();
    }

    /**
     * @return Get the height in px that the sheet is offset due to the browser controls.
     */
    float getOffsetFromBrowserControls() {
        if (mSheetContent == null || !mSheetContent.hideOnScroll()) return 0;

        // We only care about peek/half state.
        int state = getSheetState();

        // Returns non-zero offset for the opening animation. This keeps the animation running
        // below the bottom of the screen.
        if (mAlwaysFullWidth
                && state == SheetState.SCROLLING
                && mTargetState == SheetState.PEEK
                && mBrowserControlsHiddenRatio == MAX_HEIGHT_RATIO) {
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
        int bottomInset = getEdgeToEdgeBottomInset();

        // The browser controls offset is added here so that the sheet's toolbar behaves like the
        // browser controls do.
        float translationY =
                (mContainerHeight - mCurrentOffsetPx)
                        + getOffsetFromBrowserControls()
                        - (mTargetState == SheetState.HIDDEN ? 0 : bottomInset);

        // Ensure we don't over translate the bottom container.
        translationY = Math.max(0, translationY);

        updateViewport();
        boolean translationChanged = !MathUtils.areFloatsEqual(translationY, getTranslationY());
        boolean heightNeedsUpdate = false;
        if (isFullHeightResizeContent()) {
            @Px float minContentHeight = getSheetHeightForState(SheetState.HALF);
            @Px int newHeight = (int) Math.max(minContentHeight, mCurrentOffsetPx);
            newHeight = Math.min(mVisibleViewportRect.height(), newHeight);
            var params = mBottomSheetContentContainer.getLayoutParams();
            if (params != null && params.height != newHeight) {
                heightNeedsUpdate = true;
            }
        }

        if (isSheetOpen() && !translationChanged && !heightNeedsUpdate) return;

        setTranslationY(translationY);

        updateContentContainerHeight();

        // The snackbar is anchored to the bottom of the BottomSheet, so it needs to be translated
        // to the inverse of the BottomSheet's translation so it remains visible onscreen.
        if (mSnackbarContainer != null) {
            mSnackbarContainer.setTranslationY(-translationY);
        }

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

    /** Returns the ratio of the height of the screen that the hidden state is. */
    @VisibleForTesting
    float getHiddenRatio() {
        return 0;
    }

    /** Return whether the peeking state for the sheet's content is enabled. */
    boolean isPeekStateEnabled() {
        return mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DISABLED;
    }

    /** Return whether the half-height of the sheet is enabled. */
    private boolean isHalfStateEnabled() {
        if (mSheetContent == null) return false;

        // Half state is invalid on small screens, when wrapping content at full height, and when
        // explicitly disabled.
        return !isSmallScreen()
                && mSheetContent.getHalfHeightRatio() != HeightMode.DISABLED
                && mSheetContent.getFullHeightRatio() != HeightMode.WRAP_CONTENT;
    }

    /** Return whether the height mode for the full state is WRAP_CONTENT. */
    private boolean isFullHeightWrapContent() {
        return mSheetContent != null
                && mSheetContent.getFullHeightRatio() == HeightMode.WRAP_CONTENT;
    }

    /** Return whether the height mode for the full state is RESIZE_CONTENT. */
    private boolean isFullHeightResizeContent() {
        return mSheetContent != null
                && isHalfStateEnabled()
                && mSheetContent.getFullHeightRatio() == HeightMode.RESIZE_CONTENT;
    }

    /** Returns the resolved PEEK height in pixels for the current content. */
    public int getPeekHeightPx() {
        if (mContainerHeight <= 0 || !isPeekStateEnabled()) return 0;

        // If the content has a custom peek ratio set, use that instead of computing one.
        if (mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DEFAULT) {
            assert mSheetContent.getPeekHeight() != HeightMode.WRAP_CONTENT
                    : "The peek mode can't wrap content.";
            int peekHeight = mSheetContent.getPeekHeight();
            assert peekHeight > 0 : "Custom peek height must be positive.";
            // If the max sheet height is smaller than the custom peek height (e.g. when entering
            // Picture-in-Picture mode where the window shrinks dynamically, or LFF desktop modes
            // where top gaps exist), we cap the peek height to the max sheet height instead of
            // throwing an AssertionError. This gracefully allows the bottom sheet to occupy the
            // max allowed size rather than crashing the app.
            if (peekHeight > getMaxSheetHeight()) {
                Log.w(
                        TAG,
                        "Custom peek height (%d) exceeds max sheet height (%d), capping to"
                                + " max sheet height.",
                        peekHeight,
                        getMaxSheetHeight());
                peekHeight = getMaxSheetHeight();
            }
            return peekHeight;
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
                                        getMaxSheetHeight(), MeasureSpec.AT_MOST));
                        toolbarHeight = toolbarView.getMeasuredHeight();
                    }
                }
            }
        }
        return toolbarHeight;
    }

    /** Returns the ratio of the height of the screen that the peeking state is. */
    public float getPeekRatio() {
        if (mContainerHeight <= 0) return 0;
        return getPeekHeightPx() / (float) mContainerHeight;
    }

    private @Nullable View getToolbarView() {
        return mSheetContent != null && mSheetContent.getToolbarView() != null
                ? mSheetContent.getToolbarView()
                : null;
    }

    /** @return The ratio of the height of the screen that the half expanded state is. */
    @VisibleForTesting
    float getHalfRatio() {
        if (mContainerHeight <= 0 || !isHalfStateEnabled()) return 0;

        float customHalfRatio = assumeNonNull(mSheetContent).getHalfHeightRatio();
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
            return Math.min(getMaxSheetHeight(), mContentDesiredHeight) / getMaxSheetHeight();
        } else if (isFullHeightResizeContent()) {
            return MAX_HEIGHT_RATIO;
        }

        // If the customFullRatio is RESIZE_CONTENT, but half height is not enabled, set the full
        // ratio to 1.0f.
        return customFullRatio == HeightMode.DEFAULT || customFullRatio == HeightMode.RESIZE_CONTENT
                ? 1
                : customFullRatio;
    }

    /** @return The height of the container that the bottom sheet exists in. */
    public float getSheetContainerHeight() {
        return mContainerHeight;
    }

    /**
     * @return The width of the container that the bottom sheet exists in.
     */
    public float getSheetContainerWidth() {
        return mContainerWidth;
    }

    /** Return the background color of the sheet. */
    @ColorInt
    int getSheetBackgroundColor() {
        return mSheetBgColor;
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

        updateBackgroundColor();
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
        if (getCurrentSheetContent() != null) {
            @StringRes int resId = getAccessibilityStringIdForState(state);
            updateA11yPaneTitle(getResources().getString(resId));
        }

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

    private @StringRes int getAccessibilityStringIdForState(@SheetState int state) {
        assert getCurrentSheetContent() != null : "Sheet content cannot be null";
        switch (state) {
            case SheetState.PEEK:
                return getCurrentSheetContent().getSheetClosedAccessibilityStringId();
            case SheetState.HALF:
                return getCurrentSheetContent().getSheetHalfHeightAccessibilityStringId();
            case SheetState.FULL:
                return getCurrentSheetContent().getSheetFullHeightAccessibilityStringId();
            case SheetState.HIDDEN:
                return getCurrentSheetContent().getSheetHiddenAccessibilityStringId();
            default:
                assert false : "Invalid sheet state: " + state;
                return Resources.ID_NULL;
        }
    }

    /**
     * @return The target state that the sheet is moving to during animation. If the sheet is
     *     stationary or a target state has not been determined, SheetState.NONE will be returned.
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
     *
     * @param state The current state of the sheet.
     * @param reason The reason the state is changing if any.
     */
    @VisibleForTesting
    void setInternalCurrentState(@SheetState int state, @StateChangeReason int reason) {
        if (state == mCurrentState) return;

        // If we somehow got here with null content, force the sheet to close without animation.
        // See https://crbug.com/1126872 for more information.
        if (getCurrentSheetContent() == null && state != SheetState.HIDDEN) {
            Throwable throwable =
                    new Throwable(
                            "This is not a crash. See https://crbug.com/1126872 for details.");
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> assumeNonNull(sExceptionReporter).onResult(throwable));

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
            assumeNonNull(getCurrentSheetContent());

            // TalkBack will announce the pane title via sendPaneChangeAccessibilityEvent and
            // shift focus when the state settles. We set the focusability here so it is ready
            // when the pane change event is dispatched below. We avoid setting a container-level
            // contentDescription on BottomSheet so that non-interactive descendant views inside
            // the sheet remain discoverable to screen readers during linear navigation.
            setFocusable(true);
            setFocusableInTouchMode(true);
            if (getFocusedChild() == null) requestFocus();
        }

        sendPaneChangeAccessibilityEvent(mCurrentState != SheetState.HIDDEN);

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
    @Nullable BottomSheetContent getCurrentSheetContent() {
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

        return getRatioForState(state) * getMaxSheetHeight();
    }

    /**
     * @return The max possible height that the sheet can be.
     */
    private int getMaxSheetHeight() {
        if (isLargeFormFactorUiEnabled()) {
            // Clamp the height to leave an empty gap at the top of the window equal to the
            // desktop bottom margin (24dp).
            int topGap =
                    getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
            return Math.max(0, mContainerHeight - getContainerBottomMargin() - topGap);
        }
        return mContainerHeight;
    }

    @VisibleForTesting
    boolean isLargeFormFactorUiEnabled() {
        return mIsLargeFormFactor
                && mSheetContent != null
                && mSheetContent.supportsLargeFormFactor();
    }

    /**
     * @return The maximum width of the bottom sheet based on its current state and container.
     */
    public int getMaxSheetWidth() {
        if (!mAlwaysFullWidth) {
            if (isLargeFormFactorUiEnabled()) {
                int width =
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.bottom_sheet_large_form_factor_width);
                // Clamp the sheet's width to ensure a dedicated 16dp horizontal gap from the edge
                // of the window when it becomes constrained.
                int edgeGap =
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.bottom_sheet_large_form_factor_edge_gap);
                return Math.max(0, Math.min(width, mContainerWidth - 2 * edgeGap));
            }
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
        assumeNonNull(mSheetContent)
                .getContentView()
                .measure(
                        MeasureSpec.makeMeasureSpec(getMaxSheetWidth(), MeasureSpec.EXACTLY),
                        MeasureSpec.makeMeasureSpec(getMaxSheetHeight(), MeasureSpec.AT_MOST));
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
        for (@SheetState int i = smallestExpandingState - 1; i > largestCollapsingState; i--) {
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
    protected void onSheetContentChanged(final @Nullable BottomSheetContent content) {
        mSheetContent = content;
        resetCachedKeyboardState();

        boolean shouldLongPressMoveSheet =
                content == null ? false : content.shouldLongPressMoveSheet();
        mGestureDetector.setShouldLongPressMoveSheet(shouldLongPressMoveSheet);

        updateContentContainerHeight();

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
        // Update the color before notify the observers, as some might read the sheet bg color.
        if (isLargeFormFactorUiEnabled()) {
            // This mode uses a custom shadow drawable that spans around all edges of the sheet.
            // We disable child clipping in the container so this shadow doesn't get
            // abruptly cut off at the bounding box of the sheet content.
            mSheetContainer.setClipChildren(false);
            mSheetBackground.setBackgroundResource(R.drawable.bottom_sheet_desktop_background);
            mSheetBackground.setClipToOutline(true);
            setBottomMargin(0);

            // In this framework, "modal" implies the sheet uses a background scrim that blocks
            // background interaction (acting as an implicit tap-to-dismiss area). Sheets
            // that opt-out of this standard scrim (e.g., non-modal) must be provided an explicit
            // 'X' close button on large form factor environments to ensure a clear dismissal path.
            boolean showCloseButton = content != null && content.hasCustomScrimLifecycle();
            mCloseButton.setVisibility(showCloseButton ? View.VISIBLE : View.GONE);
            mCloseButton.setOnClickListener(
                    v -> setSheetState(SheetState.HIDDEN, true, StateChangeReason.CLOSE_BUTTON));
            if (mFallbackShadowLayer != null) {
                mFallbackShadowLayer.setVisibility(View.GONE);
            }
        } else if (mIsLargeFormFactor) {
            mCloseButton.setVisibility(View.GONE);
            if (mFallbackShadowLayer != null) {
                mSheetBackground.setBackgroundResource(R.drawable.bottom_sheet_background);
                mSheetBackground.setClipToOutline(false);
                mFallbackShadowLayer.setVisibility(View.VISIBLE);
                mShadowLayer.setBackgroundResource(0);
                mShadowLayer.setPadding(0, 0, 0, 0);
            }
        }
        updateBackgroundColor();
        updateBackgroundGlow();
        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
        mToolbarHolder.setBackgroundColor(Color.TRANSPARENT);
    }

    private void updateContentContainerHeight() {
        ViewGroup.LayoutParams params = mBottomSheetContentContainer.getLayoutParams();
        if (params == null) return;

        updateViewport();

        if (isFullHeightResizeContent()) {
            @Px float minContentHeight = getSheetHeightForState(SheetState.HALF);
            @Px int newHeight = (int) Math.max(minContentHeight, mCurrentOffsetPx);
            newHeight = Math.min(mVisibleViewportRect.height(), newHeight);
            if (params.height != newHeight) {
                params.height = newHeight;
                mBottomSheetContentContainer.setLayoutParams(params);
            }
        } else {
            int targetHeight =
                    isLargeFormFactorUiEnabled()
                            ? (int) getSheetHeightForState(SheetState.FULL)
                            : ViewGroup.LayoutParams.MATCH_PARENT;
            if (params.height != targetHeight) {
                params.height = targetHeight;
                mBottomSheetContentContainer.setLayoutParams(params);
            }

            @Px int viewportBottomInset = getViewportBottomInset();
            if (mBottomSheetContentContainer.getPaddingBottom() != viewportBottomInset) {
                mBottomSheetContentContainer.setPadding(
                        mBottomSheetContentContainer.getPaddingLeft(),
                        mBottomSheetContentContainer.getPaddingTop(),
                        mBottomSheetContentContainer.getPaddingRight(),
                        viewportBottomInset);
            }
        }

        int targetBgHeight =
                isLargeFormFactorUiEnabled() ? params.height : ViewGroup.LayoutParams.MATCH_PARENT;
        ViewGroup.LayoutParams bgParams = mSheetBackground.getLayoutParams();
        if (bgParams != null && bgParams.height != targetBgHeight) {
            bgParams.height = targetBgHeight;
            mSheetBackground.setLayoutParams(bgParams);
        }

        updateCurtainHeight();
    }

    private void updateViewport() {
        assert mWindow != null;

        View decorView = mWindow.getDecorView();
        @Px int decorWidth = decorView.getWidth();
        @Px int decorHeight = decorView.getHeight();

        WindowInsetsCompat insets = mInsetObserver.getLastRawWindowInsets();
        if (insets == null) {
            mWindow.getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
            mVisibleViewportRect.bottom =
                    Math.min(
                            mVisibleViewportRect.bottom,
                            decorView.getBottom() - getEdgeToEdgeBottomInset());
            mVisibleViewportRect.bottom = Math.max(mVisibleViewportRect.bottom, 0);
            return;
        }

        Insets combinedInsets =
                insets.getInsets(
                        WindowInsetsCompat.Type.ime() | WindowInsetsCompat.Type.systemBars());
        @Px int bottomInset = Math.max(combinedInsets.bottom, getEdgeToEdgeBottomInset());
        bottomInset = Math.min(bottomInset, decorHeight);

        mVisibleViewportRect.set(
                combinedInsets.left,
                combinedInsets.top,
                decorWidth - combinedInsets.right,
                decorHeight - bottomInset);
    }

    private void updateCurtainHeight() {
        assert mWindow != null;
        @Px int maxWindowHeight = mWindow.getDecorView().getHeight();
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mKeyboardCurtain.getLayoutParams();
        if (params.height != maxWindowHeight) {
            params.height = maxWindowHeight;
            mKeyboardCurtain.setTranslationY(maxWindowHeight);
            mKeyboardCurtain.setLayoutParams(params);
        }
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

    @Px
    int getContainerBottomMargin() {
        return mBottomMargin;
    }

    void setBottomMargin(@Px int bottomMargin) {
        // Enforce the baseline visual requirements for large form factor devices: ensure the sheet
        // physically floats above the logical bottom by attaching a rigid bottom margin offset.
        if (isLargeFormFactorUiEnabled()) {
            bottomMargin +=
                    getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
        }

        // TODO(crbug.com/521433079): Should early return if this doesn't change. Leaving for now to
        // ensure we don't introduce subtle client regressions.
        boolean bottomMarginChanged = mBottomMargin != bottomMargin;

        mBottomMargin = bottomMargin;
        MarginLayoutParams layoutParams = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        layoutParams.bottomMargin = mBottomMargin;
        mSheetContainer.setLayoutParams(layoutParams);

        if (!bottomMarginChanged) return;
        for (BottomSheetObserver obs : mObservers) {
            obs.onContainerBottomMarginChanged(bottomMargin);
        }
    }

    void onSheetBackgroundColorOverrideChanged() {
        updateBackgroundColor();
        for (BottomSheetObserver o : mObservers) {
            o.onSheetBackgroundColorOverrideChanged();
        }
    }

    @VisibleForTesting
    void updateBackgroundColor() {
        if (mSheetContent == null) return;

        if (mSheetContent.hasSolidBackgroundColor()) {
            int overrideColor = mSheetContent.getSheetBackgroundColorOverride();
            if (overrideColor != Color.TRANSPARENT) {
                updateSheetBgColorTint(overrideColor);
                return;
            }
        }

        int colorNonModal = getNonModalBottomSheetBgColor(getContext());
        int colorModal = getModalBottomSheetBgColor(getContext());

        // Calculate the color based on the ratio between PEEK / FULL state.
        float maxOffset = getMaxOffsetPx();
        float minOffset = getPeekRatio() * mContainerHeight;

        boolean isResizableSheet = isHalfStateEnabled() || isPeekStateEnabled();
        if (!isResizableSheet || maxOffset <= minOffset || colorModal == colorNonModal) {
            int newColor = mSheetContent.hasCustomScrimLifecycle() ? colorNonModal : colorModal;
            updateSheetBgColorTint(newColor);
            return;
        }

        float currentOffset = getCurrentOffsetPx();
        float colorRatio = Math.max(0, currentOffset - minOffset) / (maxOffset - minOffset);
        int newColor =
                ColorUtils.overlayColor(
                        /* baseColor= */ colorNonModal, /* overlayColor= */ colorModal, colorRatio);
        updateSheetBgColorTint(newColor);
    }

    private void updateSheetBgColorTint(@ColorInt int newColor) {
        if (mSheetBgColor == newColor) return;
        mSheetBgColor = newColor;
        ColorStateList tint = ColorStateList.valueOf(mSheetBgColor);
        mSheetBackground.setBackgroundTintList(tint);
        mKeyboardCurtain.setBackgroundTintList(tint);
    }

    @VisibleForTesting
    void updateBackgroundGlow() {
        if (mSheetContent == null || mShadowLayer == null) return;

        GlowSpec spec = mSheetContent.getSheetBackgroundGlowSpecOverride();
        if (spec == null) {
            spec = DEFAULT_GLOW_SPEC;
        }

        // Use the fallback shadow layer if we are on a desktop device (where the fallback
        // layer is inflated) but the current sheet has opted out of the new bottom sheet UI.
        View shadowLayer =
                (mFallbackShadowLayer != null && !isLargeFormFactorUiEnabled())
                        ? mFallbackShadowLayer
                        : mShadowLayer;

        if (isLargeFormFactorUiEnabled()) {
            mShadowLayer.setBackgroundResource(R.drawable.popup_bg_shadow_16dp);
            int size =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
            MarginLayoutParams lp = (MarginLayoutParams) mShadowLayer.getLayoutParams();
            if (lp != null) {
                // The shadow drawable actually holds visual pixels that extend outwards
                // further than the sheet itself. By applying negative margins equivalent
                // to the shadow dimension, we stretch the bounds of the shadow layer to
                // accommodate drawing the shadow fully without shrinking the inner content.
                lp.setMargins(-size, -size, -size, -size);
                mShadowLayer.setLayoutParams(lp);
            }
        } else {
            MarginLayoutParams lp = (MarginLayoutParams) mShadowLayer.getLayoutParams();
            if (lp != null) {
                lp.setMargins(0, 0, 0, 0);
                mShadowLayer.setLayoutParams(lp);
            }
            if (mFallbackShadowLayer != null) {
                mShadowLayer.setBackgroundResource(0);
            }

            if (spec.equals(DEFAULT_GLOW_SPEC)) {
                shadowLayer.setBackgroundTintList(null);
            } else {
                shadowLayer.setBackgroundTintList(ColorStateList.valueOf(spec.color));
            }
            shadowLayer.setBackgroundResource(R.drawable.top_round_shadow);
            int size;
            if (spec.size == GlowSpec.ShadowSize.LONG) {
                size =
                        getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length_large);
            } else {
                size =
                        getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length);
            }
            if (shadowLayer instanceof ShadowLayerView) {
                ((ShadowLayerView) shadowLayer).setShadowLength(size);
            }
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

    private void updateA11yPaneTitle(CharSequence msg) {
        // Set the pane title for the bottom sheet view.
        ViewCompat.setAccessibilityPaneTitle(this, msg);
    }

    // Suppressing AccessibilityFocus: The bottom sheet uses translationY for animations rather than
    // standard visibility changes, which causes the Android accessibility framework to fail at
    // automatically shifting focus to the newly opened pane. We must force focus here to ensure
    // screen readers don't get stuck on background elements (e.g. the toolbar) when the sheet
    // opens.
    @SuppressWarnings("AccessibilityFocus")
    private void sendPaneChangeAccessibilityEvent(boolean isShowing) {
        AccessibilityEvent event =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        if (isShowing) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_APPEARED);
        } else {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_PANE_DISAPPEARED);
        }
        CharSequence paneTitle = ViewCompat.getAccessibilityPaneTitle(this);
        if (paneTitle != null) {
            event.getText().add(paneTitle);
        }
        event.setSource(this);
        AccessibilityState.sendAccessibilityEvent(event);
        if (isShowing) {
            this.post(
                    () -> {
                        this.performAccessibilityAction(
                                AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS, null);
                    });
        }
    }

    private void resetCachedKeyboardState() {
        mStateBeforeKeyboardShown = SheetState.NONE;
        if (mKeyboardToken != TokenHolder.INVALID_TOKEN) {
            mKeyboardTokenHolder.releaseToken(mKeyboardToken);
            mKeyboardToken = TokenHolder.INVALID_TOKEN;
        }
    }

    /**
     * WARNING: This destroys the state of the BottomSheet. Only use in tests and only use once.
     * Puts the sheet into a scrolling state that can't be reached in tests otherwise.
     *
     * @param sheetHeightInPx The height in px that the sheet should be "scrolled" to.
     * @param yUpwardsVelocity The sheet's upwards y velocity when reaching the scrolled height.
     * @return The state the bottom sheet would target when the scrolling ends.
     */
    @SheetState
    int forceScrollingStateForTesting(float sheetHeightInPx, float yUpwardsVelocity) {
        mScrollingStartState = mCurrentState;
        mCurrentState = SheetState.SCROLLING;
        return getTargetSheetState(sheetHeightInPx, yUpwardsVelocity);
    }

    void setSheetContainerForTesting(ViewGroup sheetContainer) {
        mSheetContainer = sheetContainer;
        mContainerHeight = sheetContainer.getHeight();
    }

    void setSheetBackgroundForTesting(View sheetBackground) {
        mSheetBackground = sheetBackground;
    }

    void setShadowLayerForTesting(View shadowLayer) {
        mShadowLayer = shadowLayer;
    }

    void setToolbarHolderForTesting(TouchRestrictingFrameLayout toolbarHolder) {
        mToolbarHolder = toolbarHolder;
    }

    void setBottomSheetContentContainerForTesting(
            TouchRestrictingFrameLayout bottomSheetContentContainer) {
        mBottomSheetContentContainer = bottomSheetContentContainer;
    }

    void setEdgeToEdgeBottomInsetSupplierForTesting(
            Supplier<Integer> edgeToEdgeBottomInsetSupplier) {
        mEdgeToEdgeBottomInsetSupplier = edgeToEdgeBottomInsetSupplier;
    }

    Rect getVisibleViewportRectForTesting() {
        return mVisibleViewportRect;
    }

    /**
     * Get the color to use for bottom sheet that's shown on a scrim. The sheet on scrim has
     * different color based on light / dark theme, since the scrim can cause contrast issue between
     * the sheet background and the scrim behind.
     *
     * @param context The {@link Context} used to retrieve attrs, colors, and dimens.
     * @return The {@link ColorInt} for the background of a bottom sheet showing on a scrim
     */
    private static @ColorInt int getModalBottomSheetBgColor(Context context) {
        return ContextCompat.getColor(context, R.color.bottom_sheet_bg_color);
    }

    /**
     * Get the color to use for non-modal bottom sheet.
     *
     * @param context The {@link Context} used to retrieve attrs, colors, and dimens.
     * @return The {@link ColorInt} for the background of a bottom sheet showing on a scrim
     */
    private static @ColorInt int getNonModalBottomSheetBgColor(Context context) {
        return SemanticColorUtils.getColorSurface(context);
    }

    boolean hasKeyboardTokenForTesting() {
        return mKeyboardToken != TokenHolder.INVALID_TOKEN;
    }

    @SheetState
    int getStateBeforeKeyboardShownForTesting() {
        return mStateBeforeKeyboardShown;
    }
}
