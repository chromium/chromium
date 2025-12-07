// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.os.Handler;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Controller responsible for toggling the "mini origin" presentation of the browsing mode toolbar.
 * This state, which is not always active, has a reduced height and shows only the url bar with its
 * text at a reduced size.
 */
@NullMarked
public class MiniOriginBarController implements Observer {

    // Quantity to divide progress % by when deciding the final scale factor. We want our final
    // scale to be 0.75, so we divide progress by 4 and subtract that quantity from 1.0.
    static final float LOCATION_BAR_SCALE_DENOMINATOR = 4;
    static final float LOCATION_BAR_FINAL_SCALE = 0.75f;

    @IntDef({
        MiniOriginState.NOT_READY,
        MiniOriginState.READY,
        MiniOriginState.ANIMATING,
        MiniOriginState.SHOWING,
        MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface MiniOriginState {
        // The mini origin bar isn't showing and can't show even if a form field is focused.
        int NOT_READY = 0;
        // The mini origin bar isn't showing but can show if the soft keyboard shows.
        int READY = 1;
        // The mini origin bar is animating to either show or hide itself.
        int ANIMATING = 2;
        // The mini origin bar is showing at its fully minimized size.
        int SHOWING = 3;
        // The mini origin bar is showing at its fully minimized size and is stacked on top of a
        // keyboard accessory sheet.
        int SHOWING_WITH_ACCESSORY_SHEET = 4;
    }

    @IntDef({
        MiniOriginEvent.KEYBOARD_APPEARED,
        MiniOriginEvent.KEYBOARD_DISAPPEARED,
        MiniOriginEvent.KEYBOARD_ANIMATION_PREPARED,
        MiniOriginEvent.KEYBOARD_ANIMATION_ENDED,
        MiniOriginEvent.KEYBOARD_ANIMATION_CANCELLED_BY_USER,
        MiniOriginEvent.FORM_FIELD_GAINED_FOCUS,
        MiniOriginEvent.FORM_FIELD_LOST_FOCUS,
        MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP,
        MiniOriginEvent.CONTROLS_POSITION_BECAME_BOTTOM,
        MiniOriginEvent.ACCESSORY_SHEET_APPEARED,
        MiniOriginEvent.ACCESSORY_SHEET_DISAPPEARED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface MiniOriginEvent {
        int KEYBOARD_APPEARED = 0;
        int KEYBOARD_DISAPPEARED = 1;
        int KEYBOARD_ANIMATION_PREPARED = 2;
        int KEYBOARD_ANIMATION_ENDED = 3;
        int KEYBOARD_ANIMATION_CANCELLED_BY_USER = 4;
        int FORM_FIELD_GAINED_FOCUS = 5;
        int FORM_FIELD_LOST_FOCUS = 6;
        int CONTROLS_POSITION_BECAME_TOP = 7;
        int CONTROLS_POSITION_BECAME_BOTTOM = 8;
        int ACCESSORY_SHEET_APPEARED = 9;
        int ACCESSORY_SHEET_DISAPPEARED = 10;
    }

    private final LocationBar mLocationBar;
    private final FormFieldFocusedSupplier mIsFormFieldFocusedSupplier;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Callback<Boolean> mIsFormFieldFocusedObserver;
    private final KeyboardVisibilityListener mKeyboardVisibilityObserver;
    private final Context mContext;
    private final ControlContainer mControlContainer;
    private final ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final ObservableSupplier<Boolean> mIsKeyboardAccessorySheetShowing;
    private final MiniOriginWindowInsetsAnimationListener mWindowInsetsAnimationListener;
    private final Callback<Boolean> mAccessorySheetShowingObserver;
    private @MiniOriginState int mMiniOriginBarState = MiniOriginState.NOT_READY;
    private FrameLayout.LayoutParams mDefaultLocationBarLayoutParams;
    private final TouchEventObserver mTouchEventObserver;
    private final InsetObserver mInsetObserver;
    private final BooleanSupplier mIsOmniboxFocusedSupplier;
    private final int mDefaultLocationBarRightPadding;
    // The starting horizontal position of the location bar when the mini origin bar is in its
    // least-minimized state.
    private float mStartingLocationBarX;
    // The final horizontal position of the location bar when the mini origin bar is in its
    // fully-minimized state.
    private float mFinalLocationBarX;
    private boolean mShowingMiniOriginBar;

    /**
     * @param locationBar LocationBar instance used to change the presentation of e.g. the UrlBar
     *     and StatusView
     * @param isFormFieldFocusedSupplier Supplier to tell us if a form field is focused on the
     *     currently active WebContents.
     * @param keyboardVisibilityDelegate Delegate that tells us if a soft keyboard is visible
     * @param context Current context.
     * @param controlContainer Control container where the toolbar we're controlling the
     *     presentation of is hosted.
     */
    public MiniOriginBarController(
            LocationBar locationBar,
            FormFieldFocusedSupplier isFormFieldFocusedSupplier,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Context context,
            ControlContainer controlContainer,
            ObservableSupplierImpl<Boolean> suppressToolbarSceneLayerSupplier,
            BrowserControlsSizer browserControlsSizer,
            InsetObserver insetObserver,
            ObservableSupplierImpl<Integer> controlContainerTranslationSupplier,
            ObservableSupplier<Boolean> isKeyboardAccessorySheetShowing,
            BooleanSupplier isOmniboxFocusedSupplier) {
        mLocationBar = locationBar;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mContext = context;
        mControlContainer = controlContainer;
        mSuppressToolbarSceneLayerSupplier = suppressToolbarSceneLayerSupplier;
        mBrowserControlsSizer = browserControlsSizer;
        mIsKeyboardAccessorySheetShowing = isKeyboardAccessorySheetShowing;
        mInsetObserver = insetObserver;
        mIsOmniboxFocusedSupplier = isOmniboxFocusedSupplier;
        mDefaultLocationBarRightPadding = mLocationBar.getContainerView().getPaddingRight();
        mDefaultLocationBarLayoutParams =
                (FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams();
        mBrowserControlsSizer.addObserver(this);
        mWindowInsetsAnimationListener =
                new MiniOriginWindowInsetsAnimationListener(
                        keyboardVisibilityDelegate,
                        (ViewGroup) mLocationBar.getContainerView(),
                        controlContainerTranslationSupplier,
                        suppressToolbarSceneLayerSupplier,
                        () -> mShowingMiniOriginBar,
                        () -> updateMiniOriginBarState(MiniOriginEvent.KEYBOARD_ANIMATION_PREPARED),
                        (isCancellation) ->
                                updateMiniOriginBarState(
                                        isCancellation
                                                ? MiniOriginEvent
                                                        .KEYBOARD_ANIMATION_CANCELLED_BY_USER
                                                : MiniOriginEvent.KEYBOARD_ANIMATION_ENDED),
                        this::updateAnimationProgress,
                        this::waitingForImeAnimationToStart,
                        new Handler(),
                        controlContainer.getToolbarHeight()
                                - mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.mini_origin_bar_height));
        mInsetObserver.addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);

        mIsFormFieldFocusedObserver =
                (focused) -> {
                    if (mIsOmniboxFocusedSupplier.getAsBoolean()) return;
                    updateMiniOriginBarState(
                            focused
                                    ? MiniOriginEvent.FORM_FIELD_GAINED_FOCUS
                                    : MiniOriginEvent.FORM_FIELD_LOST_FOCUS);
                };
        mKeyboardVisibilityObserver =
                (showing) -> {
                    if (mIsOmniboxFocusedSupplier.getAsBoolean()) return;
                    updateMiniOriginBarState(
                            showing
                                    ? MiniOriginEvent.KEYBOARD_APPEARED
                                    : MiniOriginEvent.KEYBOARD_DISAPPEARED);
                };

        mIsFormFieldFocusedSupplier.addObserver(mIsFormFieldFocusedObserver);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityObserver);

        mTouchEventObserver =
                e -> {
                    if (mMiniOriginBarState != MiniOriginState.SHOWING
                            && mMiniOriginBarState != MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET
                            && mMiniOriginBarState != MiniOriginState.ANIMATING) return false;
                    // Suppress all clicks during animation as they are 1) unlikely to be
                    // intentional and 2) difficult to cleanly handle.
                    if (mMiniOriginBarState == MiniOriginState.ANIMATING) return true;
                    boolean isDownEvent = e.getActionMasked() == MotionEvent.ACTION_DOWN;
                    if (!isDownEvent) return false;
                    mIsFormFieldFocusedSupplier.resetAndHideKeyboard();
                    return true;
                };
        controlContainer.addTouchEventObserver(mTouchEventObserver);

        mAccessorySheetShowingObserver =
                (showing) ->
                        updateMiniOriginBarState(
                                showing
                                        ? MiniOriginEvent.ACCESSORY_SHEET_APPEARED
                                        : MiniOriginEvent.ACCESSORY_SHEET_DISAPPEARED);
        mIsKeyboardAccessorySheetShowing.addObserver(mAccessorySheetShowingObserver);
    }

    private void updateMiniOriginBarState(@MiniOriginEvent int event) {
        @MiniOriginState int newMiniOriginState = getNewMiniOriginState(event);
        if (newMiniOriginState == mMiniOriginBarState) return;

        transitionToNewState(newMiniOriginState);
    }

    private void transitionToNewState(@MiniOriginState int newMiniOriginState) {
        boolean isChangingVisibility =
                isMiniOriginBarVisibleForState(newMiniOriginState)
                        != isMiniOriginBarVisibleForState(mMiniOriginBarState);
        boolean finishedShowing = newMiniOriginState == MiniOriginState.SHOWING;
        mMiniOriginBarState = newMiniOriginState;

        if (!isChangingVisibility) {
            if (finishedShowing) setMinimizationProgress(1.0f);
            return;
        }

        if (isMiniOriginBarVisibleForState(newMiniOriginState)) {
            // Cache the location bar's layout params now, since we are about to mutate them.
            mDefaultLocationBarLayoutParams =
                    (FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams();
            showMiniOriginBar();
            if (finishedShowing) {
                setMinimizationProgress(1.0f);
            }
        } else {
            hideMiniOriginBar();
        }
    }

    private boolean isMiniOriginBarVisibleForState(@MiniOriginState int miniOriginBarState) {
        return switch (miniOriginBarState) {
            case MiniOriginState.NOT_READY, MiniOriginState.READY -> false;
            case MiniOriginState.ANIMATING,
                    MiniOriginState.SHOWING,
                    MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET -> true;
            default -> throw new IllegalStateException(
                    "Unexpected mini origin state: " + miniOriginBarState);
        };
    }

    private void showMiniOriginBar() {
        mShowingMiniOriginBar = true;
        mLocationBar.setShowOriginOnly(true);
        mLocationBar.setUrlBarUsesSmallText(true);
        mLocationBar.setShowStatusIconForSecureOrigins(false);
        mSuppressToolbarSceneLayerSupplier.set(true);
        mControlContainer.toggleLocationBarOnlyMode(true);

        int newLocationBarHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.mini_origin_bar_height);
        mControlContainer.mutateLayoutParams().height =
                newLocationBarHeight
                        + mContext.getResources()
                                .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        var minifiedLayoutParams =
                new CoordinatorLayout.LayoutParams(LayoutParams.WRAP_CONTENT, newLocationBarHeight);
        minifiedLayoutParams.gravity = Gravity.CENTER_VERTICAL;

        var locationBarView = mLocationBar.getContainerView();
        locationBarView.setLayoutParams(minifiedLayoutParams);
        int locationBarRightPadding = 0;
        locationBarView.setPadding(
                locationBarView.getPaddingLeft(),
                locationBarView.getPaddingTop(),
                locationBarRightPadding,
                locationBarView.getPaddingBottom());

        var controlContainerWidth = mControlContainer.getView().getWidth();
        locationBarView.measure(
                MeasureSpec.makeMeasureSpec(controlContainerWidth, MeasureSpec.AT_MOST),
                MeasureSpec.makeMeasureSpec(newLocationBarHeight, MeasureSpec.AT_MOST));
        mStartingLocationBarX = mDefaultLocationBarLayoutParams.leftMargin;
        float finalLocationBarWidth = locationBarView.getMeasuredWidth() * LOCATION_BAR_FINAL_SCALE;
        mFinalLocationBarX = (controlContainerWidth - finalLocationBarWidth) / 2;
    }

    private void hideMiniOriginBar() {
        mShowingMiniOriginBar = false;
        setMinimizationProgress(0.0f);
        mLocationBar.setShowOriginOnly(false);
        mLocationBar.setShowStatusIconForSecureOrigins(true);
        mSuppressToolbarSceneLayerSupplier.set(false);
        mControlContainer.toggleLocationBarOnlyMode(false);

        mControlContainer.mutateLayoutParams().height = LayoutParams.WRAP_CONTENT;

        var locationBarView = mLocationBar.getContainerView();
        locationBarView.setLayoutParams(mDefaultLocationBarLayoutParams);
        locationBarView.setPadding(
                locationBarView.getPaddingLeft(),
                locationBarView.getPaddingTop(),
                mDefaultLocationBarRightPadding,
                locationBarView.getPaddingBottom());
        mLocationBar.setUrlBarUsesSmallText(false);
    }

    public void destroy() {
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityObserver);
        mIsFormFieldFocusedSupplier.removeObserver(mIsFormFieldFocusedObserver);
        mIsKeyboardAccessorySheetShowing.removeObserver(mAccessorySheetShowingObserver);
        mBrowserControlsSizer.removeObserver(this);
        mInsetObserver.removeWindowInsetsAnimationListener(mWindowInsetsAnimationListener);
    }

    @Override
    public void onControlsPositionChanged(int controlsPosition) {
        updateMiniOriginBarState(
                controlsPosition == ControlsPosition.BOTTOM
                        ? MiniOriginEvent.CONTROLS_POSITION_BECAME_BOTTOM
                        : MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP);
    }

    private boolean waitingForImeAnimationToStart() {
        return !mIsOmniboxFocusedSupplier.getAsBoolean()
                && (mMiniOriginBarState == MiniOriginState.READY
                        || mMiniOriginBarState == MiniOriginState.SHOWING);
    }

    /**
     * Gets the next state of the mini origin bar considering current state and an event. This logic
     * assumes that: 1. Form field focus changes precede the associated keyboard visibility change
     * 2. Keyboard animation prepare events precede the associated keyboard visibility change 3.
     * Multiple keyboard visibility change events can fire in the course of a single animation
     */
    private @MiniOriginState int getNewMiniOriginState(@MiniOriginEvent int miniOriginEvent) {
        switch (mMiniOriginBarState) {
            case MiniOriginState.NOT_READY -> {
                if (mIsFormFieldFocusedSupplier.getAsBoolean()
                        && mBrowserControlsSizer.getControlsPosition() == ControlsPosition.BOTTOM
                        && !mIsOmniboxFocusedSupplier.getAsBoolean()) {
                    return isKeyboardShowing() ? MiniOriginState.SHOWING : MiniOriginState.READY;
                }
                return MiniOriginState.NOT_READY;
            }
            case MiniOriginState.READY -> {
                return switch (miniOriginEvent) {
                    case MiniOriginEvent.FORM_FIELD_LOST_FOCUS,
                            MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP -> MiniOriginState
                            .NOT_READY;
                    case MiniOriginEvent.KEYBOARD_ANIMATION_PREPARED -> MiniOriginState.ANIMATING;
                    case MiniOriginEvent.KEYBOARD_APPEARED ->
                    // Skip our animation if we get a keyboard appearance event before the animation
                    // prepare signal.
                    MiniOriginState.SHOWING;
                    default -> MiniOriginState.READY;
                };
            }
            case MiniOriginState.ANIMATING -> {
                return switch (miniOriginEvent) {
                    case MiniOriginEvent.FORM_FIELD_LOST_FOCUS,
                            MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP ->
                            MiniOriginState.NOT_READY;
                    // A predictive back hide animation for the keyboard can be cancelled,
                    // ending in a transient state where the keyboard insets are not applied but
                    // it remains visible. To avoid visual glitches from hiding and reshowing,
                    // we stay in the showing state.
                    case MiniOriginEvent.KEYBOARD_ANIMATION_CANCELLED_BY_USER ->
                            MiniOriginState.SHOWING;
                    case MiniOriginEvent.KEYBOARD_ANIMATION_ENDED ->
                            isKeyboardShowing() && !mIsOmniboxFocusedSupplier.getAsBoolean()
                                    ? MiniOriginState.SHOWING
                                    : MiniOriginState.READY;
                    case MiniOriginEvent.ACCESSORY_SHEET_APPEARED ->
                            MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET;
                    default -> MiniOriginState.ANIMATING;
                };
            }
            case MiniOriginState.SHOWING -> {
                return switch (miniOriginEvent) {
                    case MiniOriginEvent.ACCESSORY_SHEET_APPEARED -> MiniOriginState
                            .SHOWING_WITH_ACCESSORY_SHEET;
                    case MiniOriginEvent.FORM_FIELD_LOST_FOCUS -> isKeyboardShowing()
                            ? MiniOriginState.SHOWING
                            : MiniOriginState.NOT_READY;
                    case MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP -> MiniOriginState.NOT_READY;
                    case MiniOriginEvent.KEYBOARD_ANIMATION_PREPARED -> MiniOriginState.ANIMATING;
                    case MiniOriginEvent.KEYBOARD_DISAPPEARED ->
                    // Skip our animation if we get a keyboard disappearance event before the
                    // animation prepare signal.
                    mIsFormFieldFocusedSupplier.getAsBoolean()
                            ? MiniOriginState.READY
                            : MiniOriginState.NOT_READY;
                    default -> MiniOriginState.SHOWING;
                };
            }
            case MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET -> {
                return switch (miniOriginEvent) {
                    case MiniOriginEvent.CONTROLS_POSITION_BECAME_TOP,
                            MiniOriginEvent.FORM_FIELD_LOST_FOCUS -> MiniOriginState.NOT_READY;
                    case MiniOriginEvent.ACCESSORY_SHEET_DISAPPEARED -> MiniOriginState.SHOWING;
                        // We don't animate from this state because the accessory sheet is in the
                        // way.
                    default -> MiniOriginState.SHOWING_WITH_ACCESSORY_SHEET;
                };
            }
        }
        assert false : "Unrecognized initial mini origin state";
        return mMiniOriginBarState;
    }

    @MiniOriginState
    int getCurrentStateForTesting() {
        return mMiniOriginBarState;
    }

    MiniOriginWindowInsetsAnimationListener getAnimationListenerForTesting() {
        return mWindowInsetsAnimationListener;
    }

    private boolean isKeyboardShowing() {
        return mKeyboardVisibilityDelegate.isKeyboardShowing(mControlContainer.getView());
    }

    private void updateAnimationProgress(float minimizationProgress) {
        if (mMiniOriginBarState != MiniOriginState.ANIMATING) return;
        setMinimizationProgress(minimizationProgress);
    }

    private void setMinimizationProgress(float minimizationProgress) {
        float translationX =
                mStartingLocationBarX
                        + minimizationProgress * (mFinalLocationBarX - mStartingLocationBarX);
        mLocationBar.getContainerView().setTranslationX(translationX);

        float scale = 1.0f - minimizationProgress / LOCATION_BAR_SCALE_DENOMINATOR;
        mLocationBar.getContainerView().setScaleX(scale);
        mLocationBar.getContainerView().setScaleY(scale);
        mLocationBar.getContainerView().setPivotY(mLocationBar.getUrlBarHeight() / 2);
        mLocationBar.getContainerView().setPivotX(0.0f);
    }

    @VisibleForTesting
    static class MiniOriginWindowInsetsAnimationListener implements WindowInsetsAnimationListener {

        private static final int FALLBACK_ANIMATION_TIMEOUT = 600;
        private int mFinalKeyboardHeight;
        private int mMaxKeyboardHeight;
        private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
        private final ViewGroup mContainerView;
        private final ObservableSupplierImpl<Integer> mTranslationSupplier;
        private final ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier;
        private final BooleanSupplier mShowingMiniOriginBar;
        private final Runnable mOnAnimationPreparedSignal;
        private final Callback<Boolean> mAnimationEndedSignal;
        private final Callback<Float> mAnimationProgressSignal;
        private final BooleanSupplier mWaitingForAnimation;
        private @Nullable WindowInsetsAnimationCompat mAnimation;
        private boolean mIsCancelledPredictiveBack;

        private final Handler mHandler;
        // The height of the keyboard that should trigger an early end to a hide animation.
        private final int mEarlyEndingHeight;
        private final Runnable mCancelRunnable;

        MiniOriginWindowInsetsAnimationListener(
                KeyboardVisibilityDelegate keyboardVisibilityDelegate,
                ViewGroup containerView,
                ObservableSupplierImpl<Integer> translationSupplier,
                ObservableSupplierImpl<Boolean> suppressToolbarSceneLayerSupplier,
                BooleanSupplier showingMiniOriginBar,
                Runnable animationPreparedSignal,
                Callback<Boolean> animationEndedSignal,
                Callback<Float> animationProgressSignal,
                BooleanSupplier waitingForAnimation,
                Handler handler,
                int earlyEndingHeight) {
            mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
            mContainerView = containerView;
            mTranslationSupplier = translationSupplier;
            mSuppressToolbarSceneLayerSupplier = suppressToolbarSceneLayerSupplier;
            mShowingMiniOriginBar = showingMiniOriginBar;
            mOnAnimationPreparedSignal = animationPreparedSignal;
            mAnimationEndedSignal = animationEndedSignal;
            mAnimationProgressSignal = animationProgressSignal;
            mWaitingForAnimation = waitingForAnimation;
            mHandler = handler;
            mEarlyEndingHeight = earlyEndingHeight;
            mCancelRunnable = this::cancel;
        }

        @Override
        public void onPrepare(WindowInsetsAnimationCompat animation) {
            if (!mWaitingForAnimation.getAsBoolean()
                    || ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0)) {
                return;
            }

            mAnimation = animation;
            mOnAnimationPreparedSignal.run();
            mHandler.postDelayed(mCancelRunnable, getCancellationInterval(mAnimation));
        }

        @Override
        public void onStart(WindowInsetsAnimationCompat animation, BoundsCompat bounds) {
            if (animation != mAnimation) {
                if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0) {
                    return;
                }
                mAnimation = animation;
            }

            // Restart the clock on cancellation once we get a start signal. This avoids prematurely
            // cancelling mid-animation in cases where start took a long time but the animation
            // proceeds normally thereafter, e.g. if the IME takes a long time to warm up.
            mHandler.removeCallbacks(mCancelRunnable);
            mHandler.postDelayed(mCancelRunnable, getCancellationInterval(mAnimation));
            mMaxKeyboardHeight = bounds.getUpperBound().bottom;
            // In some cases, e.g. a floating keyboard, we get a notification of an inset animation
            // even though IME inset bottom will start and end at 0. There is a not a clean way to
            // handle this, so we just bail out of the animation early.
            if (mMaxKeyboardHeight == 0) {
                onEnd(animation);
                return;
            }
            // Prevent clipping so that the mini origin bar can draw in bounds allocated for the
            // keyboard; we will prevent overlap by syncing our translation to its movement in
            // onProgress.
            ViewUtils.setAncestorsShouldClipChildren(mContainerView, false, View.NO_ID);
            ViewUtils.setAncestorsShouldClipToPadding(mContainerView, false, View.NO_ID);
            mFinalKeyboardHeight =
                    mKeyboardVisibilityDelegate.isKeyboardShowing(mContainerView)
                            ? bounds.getUpperBound().bottom
                            : 0;
        }

        @Override
        public void onProgress(
                WindowInsetsCompat windowInsetsCompat, List<WindowInsetsAnimationCompat> list) {
            if (mAnimation == null) return;
            int currentKeyboardHeight =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;
            int translation = mFinalKeyboardHeight - currentKeyboardHeight;

            // Compensate for the system bars height only when hiding the keyboard.
            boolean hidingKeyboard = mFinalKeyboardHeight == 0;
            if (hidingKeyboard) {
                int systemBarsHeight =
                        windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
                translation += systemBarsHeight;
            }

            mTranslationSupplier.set(translation);
            mSuppressToolbarSceneLayerSupplier.set(
                    translation != 0 || mShowingMiniOriginBar.getAsBoolean());

            float interpolatedFraction = mAnimation.getInterpolatedFraction();
            mIsCancelledPredictiveBack =
                    mFinalKeyboardHeight == 0
                            && currentKeyboardHeight == mMaxKeyboardHeight
                            && interpolatedFraction == 1.0f;
            float minimizationFraction =
                    getMinimizationFractionForInterpolatedFraction(
                            mAnimation.getInterpolatedFraction());
            mAnimationProgressSignal.onResult(minimizationFraction);
            if (hidingKeyboard && Math.abs(translation) <= mEarlyEndingHeight) {
                onEnd(mAnimation);
            }
        }

        @Override
        public void onEnd(WindowInsetsAnimationCompat animation) {
            if (mAnimation != animation) return;
            mHandler.removeCallbacks(mCancelRunnable);
            ViewUtils.setAncestorsShouldClipChildren(mContainerView, true, ViewGroup.NO_ID);
            ViewUtils.setAncestorsShouldClipToPadding(mContainerView, true, ViewGroup.NO_ID);
            mTranslationSupplier.set(0);
            mSuppressToolbarSceneLayerSupplier.set(mShowingMiniOriginBar.getAsBoolean());

            mAnimationEndedSignal.onResult(mIsCancelledPredictiveBack);
            mIsCancelledPredictiveBack = false;
            mAnimation = null;
        }

        private void cancel() {
            if (mAnimation == null) return;
            onEnd(mAnimation);
        }

        private long getCancellationInterval(WindowInsetsAnimationCompat animationCompat) {
            return Math.max(animationCompat.getDurationMillis() * 2, FALLBACK_ANIMATION_TIMEOUT);
        }

        private float getMinimizationFractionForInterpolatedFraction(float interpolatedFraction) {
            if (mIsCancelledPredictiveBack) {
                return 1.0f;
            } else {
                return mFinalKeyboardHeight == 0
                        ? 1.0f - interpolatedFraction
                        : interpolatedFraction;
            }
        }
    }
}
