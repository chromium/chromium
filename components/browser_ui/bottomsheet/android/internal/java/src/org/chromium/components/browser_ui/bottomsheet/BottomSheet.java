// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.MAX_HEIGHT_RATIO;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
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
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
class BottomSheet extends BottomSheetView
        implements BottomSheetSwipeDetector.SwipeableBottomSheet, View.OnLayoutChangeListener {
    private static final String TAG = "BottomSheet";

    /** The desired height of a content that has just been shown or whose height was invalidated. */
    private static final float HEIGHT_UNSPECIFIED = -1.0f;

    /** A means of reporting an exception/stack without crashing. */
    private static @Nullable Callback<Throwable> sExceptionReporter;

    /** A flag to force the small screen state of the bottom sheet. */
    private static @Nullable Boolean sIsSmallScreenForTesting;

    /** Coordinates sheet state transitions, lifecycle, and event dispatch. */
    private final BottomSheetMediator mMediator;

    /** The visible rect for the screen taking the keyboard into account. */
    private final Rect mVisibleViewportRect = new Rect();

    /** The minimum distance between half and full states to allow the half state. */
    private final float mMinHalfFullDistance;

    /** The bottom margin for desktop windowing mode. */
    private final @Px int mDesktopBottomMargin;

    /** The sheet width for large form factor devices. */
    private final @Px int mLargeFormFactorWidth;

    /** The gap between the sheet and the edge of the window for large form factor devices. */
    private final @Px int mLargeFormFactorEdgeGap;

    /** The container width threshold below which narrow sheet layout is used. */
    private final @Px int mNarrowWidthThreshold;

    /** The sheet width when using narrow layout. */
    private final @Px int mNarrowWidth;

    /** The default peek height of the sheet. */
    private final @Px int mDefaultPeekHeight;

    /** For detecting scroll and fling events on the bottom sheet. */
    private final BottomSheetSwipeDetector mGestureDetector;

    /** The model managing presentation properties of the bottom sheet. */
    private final PropertyModel mModel;

    /** The view that contains the sheet. */
    private ViewGroup mSheetContainer;

    /** The height of the screen in the previous layout pass. */
    private int mPreviousScreenHeight;

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


    /** A handle to the FrameLayout that holds the snackbar of the bottom sheet. */
    private @Nullable FrameLayout mSnackbarContainer;

    /**
     * The last offset ratio sent to observers of onSheetOffsetChanged(). This is used to ensure the
     * min and max values are provided at least once (0 and 1).
     */
    private float mLastOffsetRatioSent;

    /** Whether the {@link BottomSheet} and its children should react to touch events. */
    private boolean mIsTouchEnabled;

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

    private @Px int mRequestedBottomMargin;
    private @Px int mBottomMargin;
    private @ColorInt int mSheetBgColor;

    @Override
    public boolean shouldGestureMoveSheet(MotionEvent initialEvent, MotionEvent currentEvent) {
        return mMediator.shouldGestureMoveSheet(
                currentEvent.getRawX(),
                getOffsetFromBrowserControls(),
                isHiding(),
                mContainerWidth,
                mVisibleViewportRect);
    }

    /**
     * Constructor for inflation from XML.
     *
     * @param context An Android context.
     * @param atts The XML attributes.
     */
    public BottomSheet(Context context, AttributeSet atts) {
        super(context, atts);

        Resources res = getResources();
        mMinHalfFullDistance =
                res.getDimensionPixelSize(R.dimen.bottom_sheet_min_full_half_distance);
        mDesktopBottomMargin =
                res.getDimensionPixelSize(R.dimen.bottom_sheet_desktop_bottom_margin);
        mLargeFormFactorWidth =
                res.getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_width);
        mLargeFormFactorEdgeGap =
                res.getDimensionPixelSize(R.dimen.bottom_sheet_large_form_factor_edge_gap);
        mNarrowWidthThreshold =
                res.getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width_threshold);
        mNarrowWidth = res.getDimensionPixelSize(R.dimen.bottom_sheet_narrow_width);
        mDefaultPeekHeight = res.getDimensionPixelSize(R.dimen.bottom_sheet_peek_height);
        mSheetBgColor = getNonModalBottomSheetBgColor(context);
        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);

        mModel = buildModel();
        mMediator = new BottomSheetMediator(mModel);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // Bind only once the child views exist. super.onFinishInflate() is what assigns
        // mCloseButton; binding any earlier makes every setter in BottomSheetView no-op against its
        // null guard, silently dropping properties that buildModel() pre-set.
        PropertyModelChangeProcessor.create(mModel, this, BottomSheetViewBinder::bind);
    }

    private PropertyModel buildModel() {
        return new PropertyModel.Builder(BottomSheetProperties.ALL_KEYS)
                .with(
                        BottomSheetProperties.CLOSE_BUTTON_CLICK_LISTENER,
                        v -> setSheetState(SheetState.HIDDEN, true, StateChangeReason.CLOSE_BUTTON))
                .with(BottomSheetProperties.SHEET_WIDTH_PX, ViewGroup.LayoutParams.MATCH_PARENT)
                .build();
    }

    /**
     * @param reporter A means of reporting an exception without crashing.
     */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        sExceptionReporter = reporter;
    }

    /** Called when the activity containing the {@link BottomSheet} is destroyed. */
    void destroy() {
        mIsDestroyed = true;
        mIsTouchEnabled = false;
        mMediator.destroy();
        endAnimations();
    }

    /** Immediately end all animations and null the animators. */
    void endAnimations() {
        if (mSettleAnimator != null) mSettleAnimator.end();
        mSettleAnimator = null;
    }

    /** @return Whether the sheet is in the process of hiding. */
    boolean isHiding() {
        return mSettleAnimator != null && getTargetSheetState() == SheetState.HIDDEN;
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

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // If the mouse event is in the transparent shadow area above the sheet, let it fall
        // through.
        if (!isTouchEventInUsableArea(event)) {
            return super.onGenericMotionEvent(event);
        }

        // Like onTouchEvent, act as a black hole for unhandled generic motion events
        // (e.g., hardware mouse clicks or scrolls) that land within the physical sheet.
        // This prevents them from falling through to the background WebContents.
        return true;
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        // First, check if a child view inside the sheet (like a specific button or the
        // drag handlebar) has explicitly requested a custom pointer icon (like a hand).
        PointerIcon icon = super.onResolvePointerIcon(event, pointerIndex);
        if (icon != null) {
            return icon;
        }

        // If no child cares, and the pointer is sitting in the empty usable area of the
        // bottom sheet, forcefully return the default arrow icon. This overwrites
        // the "stuck" hand cursor state bleeding up from the WebContents behind it.
        if (isTouchEventInUsableArea(event)) {
            return PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_DEFAULT);
        }

        return super.onResolvePointerIcon(event, pointerIndex);
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
        onAppHeaderHeightChanged(appHeaderHeight);
        setBottomMargin(bottomMargin);

        setHandlebarClickListener(v -> toggleSheetState());

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
                                @SheetState int currentState = getSheetState();
                                if (currentState == SheetState.HALF) {
                                    setSheetState(SheetState.FULL, false);
                                } else if (currentState == SheetState.SCROLLING
                                        && getTargetSheetState() == SheetState.HALF) {
                                    // Let the animation resume to the full height.
                                    mMediator.setTargetSheetState(SheetState.FULL);
                                }
                            }
                            invalidateContentDesiredHeight();
                            sizeAndPositionSheetInParent();

                            mMediator.notifyContainerSizeChanged(mContainerWidth, mContainerHeight);
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
                                @SheetState int targetState = getTargetSheetState();
                                if (targetState != SheetState.NONE) {
                                    cancelAnimation();
                                    createSettleAnimation(targetState, StateChangeReason.NONE);
                                } else {
                                    endAnimations();
                                    setSheetState(getSheetState(), false);
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
                        maybeCacheStateForImeAnimation(animation);
                        mMediator.notifyBeforeInsetAnimationStart();
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
                        mMediator.notifyInsetAnimationEnd();
                    }
                });

        // Listen to height changes on the toolbar.
        addToolbarLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    // Make sure the size of the layout actually changed.
                    if (bottom - top == oldBottom - oldTop && right - left == oldRight - oldLeft) {
                        return;
                    }

                    if (!mGestureDetector.isScrolling() && isRunningSettleAnimation()) return;

                    setSheetState(getSheetState(), false);
                });

        mSheetContainer.removeView(this);
    }

    private void onInsetChanged() {
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

        // If the screen height has changed, reset the cached state since it may no longer be valid.
        @Px int decorHeight = mWindow.getDecorView().getHeight();
        @SheetState
        int stateToRestore =
                mMediator.maybeRevertStateOnLayoutChange(
                        decorHeight,
                        mPreviousScreenHeight,
                        isKeyboardShowing(),
                        isFullHeightResizeContent());
        if (stateToRestore != SheetState.NONE) {
            setInternalCurrentState(SheetState.NONE, StateChangeReason.NONE);
            setSheetState(stateToRestore, /* animate= */ false);
        }

        mPreviousScreenHeight = decorHeight;
    }

    private boolean isKeyboardShowing() {
        return mInsetObserver.getSupplierForKeyboardInset().get() > 0;
    }

    private void maybeCacheStateForImeAnimation(WindowInsetsAnimationCompat animation) {
        mMediator.maybeCacheStateForImeAnimation(animation.getTypeMask(), isKeyboardShowing());
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
        BottomSheetContent content = getCurrentSheetContent();
        return content == null || content.getVerticalScrollOffset() <= 0;
    }

    @Override
    public float getCurrentOffsetPx() {
        return mCurrentOffsetPx;
    }

    @Override
    public float getMinOffsetPx() {
        return (swipeToDismissEnabled() ? getHiddenRatio() : getPeekRatio()) * getMaxSheetHeight();
    }

    /**
     * Test whether a motion event is in the area of the sheet considered to be usable (i.e. not on
     * the shadow shown above the sheet or some other decorative part of the view).
     *
     * @param event The motion event relative to the bottom sheet view.
     * @return Whether the event is considered to be in the usable area of the sheet.
     */
    public boolean isTouchEventInUsableArea(MotionEvent event) {
        return mMediator.isTouchEventInUsableArea(event.getY());
    }

    @Override
    public boolean isTouchEventInToolbar(MotionEvent event) {
        return isEventInToolbar(event);
    }

    /**
     * @return Whether flinging down hard enough will close the sheet.
     */
    private boolean swipeToDismissEnabled() {
        return mMediator.swipeToDismissEnabled();
    }

    /**
     * @return The minimum sheet state that the user can swipe to. i.e. flinging down will either
     *     close the sheet or peek it.
     */
    @SheetState
    int getMinSwipableSheetState() {
        return mMediator.getMinSwipableSheetState();
    }

    /**
     * Get the state that the bottom sheet should open to with the provided content.
     *
     * @return The minimum opened state for the current content.
     */
    @SheetState
    int getOpeningState() {
        return mMediator.getOpeningState(isSmallScreen());
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
        BottomSheetContent currentContent = getCurrentSheetContent();
        if (currentContent == content) return;

        // Remove this as listener from previous content layout and size changes.
        if (currentContent != null) {
            currentContent.getContentView().removeOnLayoutChangeListener(this);
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

        onSheetContentChanged(content);
    }

    /**
     * A notification that the sheet is exiting the peek state into one that shows content.
     *
     * @param reason The reason the sheet was opened, if any.
     */
    private void onSheetOpened(@StateChangeReason int reason) {
        if (!mMediator.onSheetOpened(reason)) return;
        mMediator.setSheetFocusable(true);
    }

    /**
     * A notification that the sheet has returned to the peeking state.
     *
     * @param reason The {@link StateChangeReason} that the sheet was closed, if any.
     */
    private void onSheetClosed(@StateChangeReason int reason) {
        if (!mMediator.onSheetClosed(reason)) return;
        mMediator.setSheetFocusable(false);
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
        mMediator.setTargetSheetState(targetState);
        mSettleAnimator =
                ValueAnimator.ofFloat(getCurrentOffsetPx(), getSheetHeightForState(targetState));
        long duration = mMediator.getSettleDuration(targetState);
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
                        if (isLargeFormFactorUiEnabled()
                                && !mIsDestroyed
                                && getSheetState() == targetState) {
                            // Re-synchronize sheet offset after observers run in
                            // setInternalCurrentState, ensuring any layout or measurement
                            // adjustments made by observers (e.g. BottomSheetListViewBase or
                            // EnhancedTargetDevicePickerView) are immediately reflected in
                            // mCurrentOffsetPx and view translation.
                            setSheetOffsetFromBottom(getSheetHeightForState(targetState), reason);
                        }
                        mMediator.setTargetSheetState(SheetState.NONE);
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
        BottomSheetContent content = getCurrentSheetContent();
        if (content == null || !content.hideOnScroll()) return 0;

        // We only care about peek/half state.
        int state = getSheetState();

        // Returns non-zero offset for the opening animation. This keeps the animation running
        // below the bottom of the screen.
        @SheetState int targetState = getTargetSheetState();
        if (mAlwaysFullWidth
                && state == SheetState.SCROLLING
                && targetState == SheetState.PEEK
                && mBrowserControlsHiddenRatio == MAX_HEIGHT_RATIO) {
            state = targetState;
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
        @SheetState int targetState = getTargetSheetState();
        boolean isSheetOpen = isSheetOpen();

        // The browser controls offset is added here so that the sheet's toolbar behaves like the
        // browser controls do.
        float translationY =
                (mContainerHeight - mCurrentOffsetPx)
                        + getOffsetFromBrowserControls()
                        - (targetState == SheetState.HIDDEN ? 0 : bottomInset);

        // Ensure we don't over translate the bottom container.
        translationY = Math.max(0, translationY);

        updateViewport();
        boolean translationChanged = !MathUtils.areFloatsEqual(translationY, getTranslationY());
        boolean heightNeedsUpdate = false;
        if (isFullHeightResizeContent()) {
            @Px int newHeight = getResizingContentContainerHeight();
            if (isContentContainerHeightDifferent(newHeight)) {
                heightNeedsUpdate = true;
            }
        }

        if (isSheetOpen && !translationChanged && !heightNeedsUpdate) return;

        mMediator.setSheetTranslationY(translationY);

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
            if (isPeekStateEnabled() && (!isSheetOpen || targetState == SheetState.PEEK)) {
                minSwipableState = SheetState.PEEK;
            }

            float minScrollableHeight = getSheetHeightForState(minSwipableState);
            boolean isAtMinHeight =
                    MathUtils.areFloatsEqual(getCurrentOffsetPx(), minScrollableHeight);
            boolean heightLessThanPeek = getCurrentOffsetPx() < minScrollableHeight;

            if (isSheetOpen && (heightLessThanPeek || isAtMinHeight)) {
                onSheetClosed(reason);
            } else if (!isSheetOpen
                    && targetState != SheetState.HIDDEN
                    && getCurrentOffsetPx() > minScrollableHeight) {
                onSheetOpened(reason);
            }
        }

        sendOffsetChangeEvents();
    }

    @Override
    public void setSheetOffset(float offset, boolean shouldAnimate) {
        cancelAnimation();
        if (getCurrentSheetContent() == null) return;

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
        return mMediator.getHiddenRatio();
    }

    /** Return whether the peeking state for the sheet's content is enabled. */
    boolean isPeekStateEnabled() {
        return mMediator.isPeekStateEnabled();
    }

    /** Return whether the half-height of the sheet is enabled. */
    private boolean isHalfStateEnabled() {
        return mMediator.isHalfStateEnabled(isSmallScreen());
    }

    /** Return whether the height mode for the full state is WRAP_CONTENT. */
    private boolean isFullHeightWrapContent() {
        return mMediator.isFullHeightWrapContent();
    }

    /** Return whether the height mode for the full state is RESIZE_CONTENT. */
    private boolean isFullHeightResizeContent() {
        BottomSheetContent content = getCurrentSheetContent();
        return content != null
                && isHalfStateEnabled()
                && content.getFullHeightRatio() == HeightMode.RESIZE_CONTENT;
    }

    private @Px int getResizingContentContainerHeight() {
        return mMediator.calculateContentContainerHeight(
                getSheetHeightForState(SheetState.HALF),
                getSheetHeightForState(SheetState.FULL),
                mCurrentOffsetPx,
                mVisibleViewportRect);
    }

    /** Returns the resolved PEEK height in pixels for the current content. */
    public int getPeekHeightPx() {
        if (mContainerHeight <= 0 || !isPeekStateEnabled()) return 0;

        BottomSheetContent content = getCurrentSheetContent();
        View toolbarView =
                (content == null || content.getPeekHeight() == HeightMode.DEFAULT)
                        ? getToolbarView()
                        : null;

        int toolbarHeight;
        if (toolbarView == null) {
            toolbarHeight = mDefaultPeekHeight;
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

        return mMediator.getPeekHeight(
                mContainerHeight, getMaxSheetHeight(), toolbarHeight, getHandlebarHeight());
    }

    /** Returns the ratio of the maximum sheet height that the peeking state is. */
    public float getPeekRatio() {
        int maxSheetHeight = getMaxSheetHeight();
        if (maxSheetHeight <= 0) return 0;
        return mMediator.getPeekRatio(maxSheetHeight, getPeekHeightPx());
    }

    private @Nullable View getToolbarView() {
        BottomSheetContent content = getCurrentSheetContent();
        return content != null ? content.getToolbarView() : null;
    }

    /**
     * @return The ratio of the height of the screen that the half expanded state is.
     */
    @VisibleForTesting
    float getHalfRatio() {
        return mMediator.getHalfRatio(mContainerHeight, isSmallScreen());
    }

    /**
     * @return The ratio of the height of the screen that the fully expanded state is.
     */
    @VisibleForTesting
    float getFullRatio() {
        BottomSheetContent content = getCurrentSheetContent();
        if (mContainerHeight <= 0 || content == null) return 0;

        float customFullRatio = content.getFullHeightRatio();
        assert customFullRatio != HeightMode.DISABLED : "The full height cannot be DISABLED.";

        if (isFullHeightWrapContent()) {
            ensureContentDesiredHeightIsComputed();
            int maxSheetHeight = getMaxSheetHeight();
            return Math.min(maxSheetHeight, mContentDesiredHeight) / maxSheetHeight;
        } else if (isFullHeightResizeContent()) {
            float maxRatioCap = content.getMaxResizeContentHeightRatio();
            if (maxRatioCap <= 0.0f || maxRatioCap > MAX_HEIGHT_RATIO) {
                return MAX_HEIGHT_RATIO;
            }
            return maxRatioCap;
        }

        // If the customFullRatio is RESIZE_CONTENT, but half height is not enabled, set the full
        // ratio to 1.0f.
        return customFullRatio == HeightMode.DEFAULT || customFullRatio == HeightMode.RESIZE_CONTENT
                ? MAX_HEIGHT_RATIO
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
        mMediator.notifySheetOffsetChanged(mLastOffsetRatioSent, getCurrentOffsetPx());
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
        @SheetState int currentState = getSheetState();
        if (state == SheetState.SCROLLING) {
            // TODO(mdjones): The isRunningSettleAnimation should hold but currently doesn't.
            assert currentState == SheetState.SCROLLING; // && isRunningSettleAnimation();
            return;
        }

        if (state == SheetState.HALF && !isHalfStateEnabled()) state = SheetState.FULL;

        cancelAnimation();
        mMediator.setTargetSheetState(state);
        if (getCurrentSheetContent() != null) {
            @StringRes int resId = getAccessibilityStringIdForState(state);
            updateA11yPaneTitle(getResources().getString(resId));
        }

        @SheetState int targetState = getTargetSheetState();
        if (animate
                && (state != currentState
                        || mCurrentOffsetPx != getSheetHeightForState(targetState))) {
            createSettleAnimation(state, reason);
        } else {
            setSheetOffsetFromBottom(getSheetHeightForState(state), reason);
            setInternalCurrentState(getTargetSheetState(), reason);
            mMediator.setTargetSheetState(SheetState.NONE);
        }
    }

    private @StringRes int getAccessibilityStringIdForState(@SheetState int state) {
        return mMediator.getAccessibilityStringIdForState(state);
    }

    /**
     * @return The target state that the sheet is moving to during animation. If the sheet is
     *     stationary or a target state has not been determined, SheetState.NONE will be returned.
     */
    int getTargetSheetState() {
        return mMediator.getTargetSheetState();
    }

    /**
     * @return The current state of the bottom sheet. If the sheet is animating, this will be the
     *     state the sheet is animating to.
     */
    @SheetState
    int getSheetState() {
        return mMediator.getSheetState();
    }

    /**
     * @return Whether the sheet is currently open.
     */
    boolean isSheetOpen() {
        return mMediator.isSheetOpen();
    }

    private void updateContainerClipping(boolean isPopup) {
        if (mSheetContainer == null) return;
        // Container child clipping must always remain false so both mobile top-shadow
        // translations and popup glow can render past bounds without being sliced.
        mSheetContainer.setClipChildren(false);
        mSheetContainer.setClipToPadding(!isPopup);
        if (mSheetContainer.getParent() instanceof ViewGroup parent) {
            parent.setClipChildren(!isPopup);
            parent.setClipToPadding(!isPopup);
        }
    }

    @Override
    public void setSheetLayoutMode(@SheetLayoutMode int mode) {
        super.setSheetLayoutMode(mode);
        mMediator.setSheetLayoutMode(mode);
        boolean isPopup = mode == SheetLayoutMode.DESKTOP_POPUP;
        setBottomMargin(mRequestedBottomMargin);
        mMediator.updateCloseButton(isPopup, getCurrentSheetContent());
        updateContainerClipping(isPopup);
    }

    private boolean isLargeFormFactorFallbackUiEnabled() {
        BottomSheetContent content = getCurrentSheetContent();
        return mIsLargeFormFactor && content != null && !content.supportsLargeFormFactor();
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
        @SheetState int currentState = getSheetState();
        if (state == currentState) return;

        BottomSheetContent content = getCurrentSheetContent();
        // If we somehow got here with null content, force the sheet to close without animation.
        // See https://crbug.com/1126872 for more information.
        if (content == null && state != SheetState.HIDDEN) {
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

        mMediator.setInternalCurrentState(state);

        if (state == SheetState.HALF || state == SheetState.FULL) {
            if (isLargeFormFactorUiEnabled() || isFullHeightResizeContent()) {
                updateContentContainerHeight();
            }
            assumeNonNull(content);

            // TalkBack will announce the pane title via sendPaneChangeAccessibilityEvent and
            // shift focus when the state settles. We set the focusability here so it is ready
            // when the pane change event is dispatched below. We avoid setting a container-level
            // contentDescription on BottomSheet so that non-interactive descendant views inside
            // the sheet remain discoverable to screen readers during linear navigation.
            mMediator.setSheetFocusable(true);
        }

        sendPaneChangeAccessibilityEvent(state != SheetState.HIDDEN);

        mMediator.notifySheetStateChanged(state, reason);
    }

    /**
     * If the animation to settle the sheet in one of its states is running.
     * @return True if the animation is running.
     */
    private boolean isRunningSettleAnimation() {
        return mSettleAnimator != null;
    }

    /**
     * @return The content currently being displayed in the sheet.
     */
    @Nullable BottomSheetContent getCurrentSheetContent() {
        return mMediator.getCurrentSheetContent();
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     *
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    @VisibleForTesting
    float getSheetHeightForState(@SheetState int state) {
        if (isFullHeightWrapContent() && state == SheetState.FULL) {
            ensureContentDesiredHeightIsComputed();
        }

        return getRatioForState(state) * getMaxSheetHeight();
    }

    /**
     * @return The max possible height that the sheet can be.
     */
    @Px
    int getMaxSheetHeight() {
        if (isLargeFormFactorUiEnabled()) {
            // Clamp the height to leave an empty gap at the top of the window equal to the
            // desktop bottom margin (24dp).
            int topGap = mDesktopBottomMargin;
            return Math.max(0, mContainerHeight - getContainerBottomMargin() - topGap);
        }
        return mContainerHeight;
    }

    @Override
    @VisibleForTesting
    public boolean isLargeFormFactorUiEnabled() {
        BottomSheetContent content = getCurrentSheetContent();
        return mIsLargeFormFactor && content != null && content.supportsLargeFormFactor();
    }

    public void toggleSheetState() {
        boolean isHalfStateEnabled = isHalfStateEnabled();
        // Early exit if the sheet only supports one open state (FULL).
        if (!isHalfStateEnabled && !isPeekStateEnabled()) return;

        @SheetState int currentState = getSheetState();
        if (currentState == SheetState.FULL) {
            // We know at least one other state is enabled here.
            // Go to HALF if enabled, otherwise it must be PEEK.
            setSheetState(
                    isHalfStateEnabled ? SheetState.HALF : SheetState.PEEK,
                    /* animate= */ true,
                    StateChangeReason.NONE);
        } else if (currentState == SheetState.HALF) {
            setSheetState(SheetState.FULL, /* animate= */ true, StateChangeReason.NONE);
        } else if (currentState == SheetState.PEEK) {
            setSheetState(
                    isHalfStateEnabled ? SheetState.HALF : SheetState.FULL,
                    /* animate= */ true,
                    StateChangeReason.NONE);
        }
    }

    /**
     * @return The maximum width of the bottom sheet based on its current state and container.
     */
    public int getMaxSheetWidth() {
        if (!mAlwaysFullWidth) {
            if (isLargeFormFactorUiEnabled()) {
                int width = mLargeFormFactorWidth;
                // Clamp the sheet's width to ensure a dedicated 16dp horizontal gap from the edge
                // of the window when it becomes constrained.
                int edgeGap = mLargeFormFactorEdgeGap;
                return Math.max(0, Math.min(width, mContainerWidth - 2 * edgeGap));
            }
            int narrowWidthThreshold = mNarrowWidthThreshold;
            if (mContainerWidth > narrowWidthThreshold) {
                return mNarrowWidth;
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
        mMediator.setSheetWidth(maxSheetWidth);
        mMediator.setSheetTranslationX(
                (LocalizationUtils.isLayoutRtl() ? -1 : 1)
                        * (mContainerWidth - maxSheetWidth)
                        / 2f);
        ViewUtils.requestLayout(this, "BottomSheet.sizeAndPositionSheetInParent");
    }

    private void ensureContentDesiredHeightIsComputed() {
        if (mContentDesiredHeight != HEIGHT_UNSPECIFIED) {
            return;
        }
        BottomSheetContent content = getCurrentSheetContent();
        View contentView = assumeNonNull(content).getContentView();
        contentView.measure(
                MeasureSpec.makeMeasureSpec(getMaxSheetWidth(), MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(getMaxSheetHeight(), MeasureSpec.AT_MOST));
        mContentDesiredHeight = contentView.getMeasuredHeight();
        if (content.showHandlebar()) {
            mContentDesiredHeight += getHandlebarHeight();
        }
    }

    private int getHandlebarHeight() {
        BottomSheetContent content = getCurrentSheetContent();
        if (content == null || !content.showHandlebar()) {
            return 0;
        }
        return getHandlebarMeasuredHeight(getMaxSheetWidth(), getMaxSheetHeight());
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
     *
     * @param observer The observer to add.
     */
    void addObserver(BottomSheetObserver observer) {
        mMediator.addObserver(observer);
    }

    /**
     * Removes an observer to the bottom sheet.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(BottomSheetObserver observer) {
        mMediator.removeObserver(observer);
    }

    BottomSheetMediator getMediatorForTesting() {
        return mMediator;
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
        return mMediator.getTargetSheetState(
                sheetHeight,
                yVelocity,
                isHalfStateEnabled(),
                isPeekStateEnabled(),
                swipeToDismissEnabled(),
                getSheetHeightForState(SheetState.PEEK),
                getSheetHeightForState(SheetState.HALF),
                getSheetHeightForState(SheetState.FULL));
    }

    public static void setSmallScreenForTesting(boolean isSmallScreen) {
        sIsSmallScreenForTesting = isSmallScreen;
        ResettersForTesting.register(() -> sIsSmallScreenForTesting = null);
    }

    public boolean isSmallScreen() {
        if (sIsSmallScreenForTesting != null) return sIsSmallScreenForTesting;

        float halfRatio = BottomSheetMediator.HALF_HEIGHT_RATIO;
        BottomSheetContent content = getCurrentSheetContent();
        if (content != null) {
            float customHalf = content.getHalfHeightRatio();
            if (customHalf > 0 && customHalf < BottomSheetMediator.HALF_HEIGHT_RATIO) {
                halfRatio = customHalf;
            }
        }

        // A small screen is defined by there being less than 140dp between half and full states.
        float fullToHalfDiff = (1 - halfRatio) * mContainerHeight;
        return fullToHalfDiff < mMinHalfFullDistance;
    }

    /**
     * Called when the sheet content has changed, to update dependent state and notify observers.
     *
     * @param content The new sheet content, or null if the sheet has no content.
     */
    protected void onSheetContentChanged(final @Nullable BottomSheetContent content) {
        mMediator.setSheetContent(content);
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
            if (getSheetState() == SheetState.HALF) {
                setSheetState(SheetState.FULL, /* animate= */ true);
            }
        }
        // Update the color before notify the observers, as some might read the sheet bg color.
        boolean isLargeFormFactorUiEnabled = isLargeFormFactorUiEnabled();
        @SheetLayoutMode int mode = SheetLayoutMode.STANDARD;
        if (isLargeFormFactorUiEnabled) {
            mode = SheetLayoutMode.DESKTOP_POPUP;
        } else if (isLargeFormFactorFallbackUiEnabled()) {
            mode = SheetLayoutMode.DESKTOP_FALLBACK;
        }

        boolean showHandlebar = content != null && content.showHandlebar();
        mMediator.setHandlebarVisible(showHandlebar);
        setHandlebarPointerIcon(
                isLargeFormFactorUiEnabled
                        ? PointerIcon.getSystemIcon(getContext(), PointerIcon.TYPE_HAND)
                        : null);
        updateContentContainerHeight();
        sizeAndPositionSheetInParent();
        updateBackgroundColor();
        setSheetLayoutMode(mode);
        mMediator.notifySheetContentChanged(content);
        setToolbarBackgroundColor(Color.TRANSPARENT);
    }

    private @SheetState int getTargetOrCurrentState() {
        @SheetState int targetState = getTargetSheetState();
        if (targetState != SheetState.NONE && targetState != SheetState.SCROLLING) {
            return targetState;
        }
        @SheetState int currentState = getSheetState();
        if (currentState != SheetState.NONE && currentState != SheetState.SCROLLING) {
            return currentState;
        }
        return SheetState.FULL;
    }

    private void updateContentContainerHeight() {
        updateViewport();

        int topMargin = getHandlebarHeight();
        mMediator.setContentTopMargin(topMargin);

        boolean isLargeFormFactorUiEnabled = isLargeFormFactorUiEnabled();
        if (isFullHeightResizeContent()) {
            mMediator.setContainerHeight(getResizingContentContainerHeight());
            setContentContainerPaddingBottom(0);
        } else {
            int targetHeight;
            if (isLargeFormFactorUiEnabled) {
                if (isFullHeightWrapContent()) {
                    targetHeight = ViewGroup.LayoutParams.WRAP_CONTENT;
                } else {
                    targetHeight =
                            (int) getSheetHeightForState(getTargetOrCurrentState()) - topMargin;
                }
            } else {
                targetHeight = ViewGroup.LayoutParams.MATCH_PARENT;
            }
            mMediator.setContainerHeight(targetHeight);

            @Px int viewportBottomInset = isLargeFormFactorUiEnabled ? 0 : getViewportBottomInset();
            setContentContainerPaddingBottom(viewportBottomInset);
        }

        int targetBgHeight =
                isLargeFormFactorUiEnabled
                        ? (int) getSheetHeightForState(SheetState.FULL)
                        : ViewGroup.LayoutParams.MATCH_PARENT;
        updateBackgroundHeight(targetBgHeight);

        updateCurtainHeight();

        if (isLargeFormFactorUiEnabled) {
            applyLargeFormFactorBackgroundBounds();
        }
    }

    /**
     * Shrinks the background and shadow to match the visible height of the sheet on LFF (desktop
     * currently).
     *
     * <p>When a user drags the sheet downward, the internal view doesn't actually resize; it just
     * gets pushed off-screen. This method visually trims the background to ensure the bottom
     * rounded corners and drop shadows stay perfectly aligned with the bottom of the window instead
     * of disappearing below it.
     */
    private void applyLargeFormFactorBackgroundBounds() {
        // The true visual height of the sheet's cosmetic wrapper.
        int visibleHeight = (int) Math.max(0, mCurrentOffsetPx);
        if (visibleHeight == 0) {
            return;
        }

        // Ensure we don't accidentally ask for a bounds size larger than the actual layout limits.
        int targetFullHeight = (int) getSheetHeightForState(SheetState.FULL);
        if (targetFullHeight > 0) {
            visibleHeight = Math.min(visibleHeight, targetFullHeight);
        }

        mMediator.setVisibleBackgroundHeight(visibleHeight);
    }

    /**
     * This is needed so that on layout changes, such as the browser resizing, or moving, the layout
     * should remain tracked for peeking sheets on large form factors.
     */
    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (isLargeFormFactorUiEnabled()) {
            // Allow the shadow to draw outside the strict layout boundaries of the container.
            // Since the shadow expands into the container's bottom margin or window insets,
            // we must also disable clipping on the parent to prevent the shadow from being sliced.
            updateContainerClipping(true);
            applyLargeFormFactorBackgroundBounds();
        }
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
        mMediator.setKeyboardCurtainHeight(maxWindowHeight);
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
        mRequestedBottomMargin = bottomMargin;
        int effectiveBottomMargin = bottomMargin;
        // Enforce the baseline visual requirements for large form factor devices: ensure the sheet
        // physically floats above the logical bottom by attaching a rigid bottom margin offset.
        if (isLargeFormFactorUiEnabled()) {
            effectiveBottomMargin += mDesktopBottomMargin;
        }

        // TODO(crbug.com/521433079): Should early return if this doesn't change. Leaving for now to
        // ensure we don't introduce subtle client regressions.
        boolean bottomMarginChanged = mBottomMargin != effectiveBottomMargin;

        mBottomMargin = effectiveBottomMargin;
        MarginLayoutParams layoutParams = (MarginLayoutParams) mSheetContainer.getLayoutParams();
        layoutParams.bottomMargin = mBottomMargin;
        mSheetContainer.setLayoutParams(layoutParams);

        if (!bottomMarginChanged) return;
        mMediator.notifyContainerBottomMarginChanged(effectiveBottomMargin);
    }

    void onSheetBackgroundColorOverrideChanged() {
        updateBackgroundColor();
        mMediator.notifySheetBackgroundColorOverrideChanged();
    }

    @VisibleForTesting
    void updateBackgroundColor() {
        if (getCurrentSheetContent() == null) return;
        Context context = getContext();
        int colorNonModal = getNonModalBottomSheetBgColor(context);
        int colorModal = getModalBottomSheetBgColor(context);

        int newColor =
                mMediator.getBackgroundColor(
                        colorNonModal,
                        colorModal,
                        getMaxOffsetPx(),
                        getPeekRatio() * getMaxSheetHeight(),
                        getCurrentOffsetPx(),
                        isSmallScreen());
        updateSheetBgColorTint(newColor);
    }

    private void updateSheetBgColorTint(@ColorInt int newColor) {
        if (mSheetBgColor == newColor) return;
        mSheetBgColor = newColor;
        mMediator.setBackgroundColor(mSheetBgColor);
    }

    private void ensureContentIsWrapped(boolean animate) {
        @SheetState int currentState = getSheetState();
        if (currentState == SheetState.HIDDEN || currentState == SheetState.PEEK) return;

        // The SCROLLING state is used when animating the sheet height or when the user is swiping
        // the sheet. If it is the latter, we should not change the sheet height.
        if (!isRunningSettleAnimation() && currentState == SheetState.SCROLLING) return;
        setSheetState(currentState, animate);
    }

    private void invalidateContentDesiredHeight() {
        mContentDesiredHeight = HEIGHT_UNSPECIFIED;
    }

    private void updateA11yPaneTitle(CharSequence msg) {
        mMediator.setAccessibilityPaneTitle(msg);
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
        mMediator.resetCachedKeyboardState();
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
        mMediator.setScrollingStartState(getSheetState());
        mMediator.setSheetStateForTesting(SheetState.SCROLLING);
        return getTargetSheetState(sheetHeightInPx, yUpwardsVelocity);
    }

    void setSheetContainerForTesting(ViewGroup sheetContainer) {
        mSheetContainer = sheetContainer;
        mContainerHeight = sheetContainer.getHeight();
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
        return mMediator.hasKeyboardTokenForTesting();
    }

    @SheetState
    int getStateBeforeKeyboardShownForTesting() {
        return mMediator.getStateBeforeKeyboardShownForTesting();
    }
}
