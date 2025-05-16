// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.ViewUtils;

import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * Controller responsible for toggling the "mini origin" presentation of the browsing mode toolbar.
 * This state, which is not always active, has a reduced height and shows only the url bar with its
 * text at a reduced size.
 */
@NullMarked
public class MiniOriginBarController implements Observer {

    private final LocationBar mLocationBar;
    private final ObservableSupplier<Boolean> mIsFormFieldFocusedSupplier;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Callback<Boolean> mIsFormFieldFocusedObserver;
    private final KeyboardVisibilityListener mKeyboardVisibilityObserver;
    private final Context mContext;
    private final ControlContainer mControlContainer;
    private final ObservableSupplierImpl<Boolean> mSuppressToolbarSceneLayerSupplier;
    private final BrowserControlsSizer mBrowserControlsSizer;
    private final BooleanSupplier mIsKeyboardAccessorySheetShowing;
    private final MiniOriginWindowInsetsAnimationListener mWindowInsetsAnimationListener;
    private boolean mShowMiniOriginBar;
    private FrameLayout.LayoutParams mDefaultLocationBarLayoutParams;
    private boolean mOriginBarClickedInSession;
    private final TouchEventObserver mTouchEventObserver;
    private final int mDefaultLocationBarRightPadding;
    private boolean mFormFieldFocusChanged;

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
            ObservableSupplier<Boolean> isFormFieldFocusedSupplier,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Context context,
            ControlContainer controlContainer,
            ObservableSupplierImpl<Boolean> suppressToolbarSceneLayerSupplier,
            BrowserControlsSizer browserControlsSizer,
            InsetObserver insetObserver,
            ObservableSupplierImpl<Integer> controlContainerTranslationSupplier,
            BooleanSupplier isKeyboardAccessorySheetShowing) {
        mLocationBar = locationBar;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mContext = context;
        mControlContainer = controlContainer;
        mSuppressToolbarSceneLayerSupplier = suppressToolbarSceneLayerSupplier;
        mBrowserControlsSizer = browserControlsSizer;
        mIsKeyboardAccessorySheetShowing = isKeyboardAccessorySheetShowing;
        mDefaultLocationBarRightPadding = mLocationBar.getContainerView().getPaddingRight();
        mDefaultLocationBarLayoutParams =
                (FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams();
        mBrowserControlsSizer.addObserver(this);
        mWindowInsetsAnimationListener =
                new MiniOriginWindowInsetsAnimationListener(
                        keyboardVisibilityDelegate,
                        (ViewGroup) mLocationBar.getContainerView(),
                        controlContainerTranslationSupplier,
                        () -> mFormFieldFocusChanged = false,
                        this::waitingForImeAnimationToStart);
        insetObserver.addWindowInsetsAnimationListener(mWindowInsetsAnimationListener);

        mIsFormFieldFocusedObserver =
                (focused) -> {
                    mFormFieldFocusChanged = true;
                    updateMiniOriginBarState();
                };
        mKeyboardVisibilityObserver = (showing) -> updateMiniOriginBarState();

        mIsFormFieldFocusedSupplier.addObserver(mIsFormFieldFocusedObserver);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityObserver);

        mTouchEventObserver =
                e -> {
                    if (!mShowMiniOriginBar) return false;
                    boolean isDownEvent = e.getActionMasked() == MotionEvent.ACTION_DOWN;
                    mOriginBarClickedInSession = isDownEvent;
                    updateMiniOriginBarState();
                    return isDownEvent;
                };
        controlContainer.addTouchEventObserver(mTouchEventObserver);
    }

    private void updateMiniOriginBarState() {
        boolean isFormFieldFocused = mIsFormFieldFocusedSupplier.get();
        boolean isKeyboardVisible =
                mKeyboardVisibilityDelegate.isKeyboardShowing(
                        mContext, mControlContainer.getView());
        boolean isToolbarBottomAnchored =
                mBrowserControlsSizer.getControlsPosition() == ControlsPosition.BOTTOM;
        if (!isFormFieldFocused || !isKeyboardVisible) mOriginBarClickedInSession = false;

        boolean showMiniOriginBar =
                !mOriginBarClickedInSession
                        && isToolbarBottomAnchored
                        && isFormFieldFocused
                        && isKeyboardVisible;
        if (showMiniOriginBar == mShowMiniOriginBar) return;

        if (showMiniOriginBar) {
            mDefaultLocationBarLayoutParams =
                    (FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams();
        }

        mShowMiniOriginBar = showMiniOriginBar;
        mLocationBar.setShowOriginOnly(mShowMiniOriginBar);
        mLocationBar.setUrlBarUsesSmallText(mShowMiniOriginBar);
        mLocationBar.setHideStatusIconForSecureOrigins(mShowMiniOriginBar);
        mSuppressToolbarSceneLayerSupplier.set(mShowMiniOriginBar);
        mControlContainer.toggleLocationBarOnlyMode(mShowMiniOriginBar);

        int newControlContainerHeight =
                showMiniOriginBar
                        ? mContext.getResources()
                                .getDimensionPixelSize(R.dimen.mini_origin_bar_height)
                        : LayoutParams.WRAP_CONTENT;
        mControlContainer.mutateLayoutParams().height = newControlContainerHeight;

        var minifiedLayoutParams =
                new FrameLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT, newControlContainerHeight, Gravity.CENTER);

        var locationBarView = mLocationBar.getContainerView();
        var locationBarLayoutParams =
                mShowMiniOriginBar ? minifiedLayoutParams : mDefaultLocationBarLayoutParams;
        locationBarView.setLayoutParams(locationBarLayoutParams);
        int locationBarRightPadding = mShowMiniOriginBar ? 0 : mDefaultLocationBarRightPadding;
        locationBarView.setPadding(
                locationBarView.getPaddingLeft(),
                locationBarView.getPaddingTop(),
                locationBarRightPadding,
                locationBarView.getPaddingBottom());
    }

    public void destroy() {
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityObserver);
        mIsFormFieldFocusedSupplier.removeObserver(mIsFormFieldFocusedObserver);
        mBrowserControlsSizer.removeObserver(this);
    }

    @Override
    public void onControlsPositionChanged(int controlsPosition) {
        updateMiniOriginBarState();
    }

    @VisibleForTesting
    boolean waitingForImeAnimationToStart() {
        return mFormFieldFocusChanged && !mIsKeyboardAccessorySheetShowing.getAsBoolean();
    }

    @VisibleForTesting
    static class MiniOriginWindowInsetsAnimationListener implements WindowInsetsAnimationListener {

        private boolean mAnimationInProgress;
        private int mFinalKeyboardHeight;
        private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
        private final ViewGroup mContainerView;
        private final ObservableSupplierImpl<Integer> mTranslationSupplier;
        private final Context mContext;
        private final Runnable mAnimationStartedSignal;
        private final BooleanSupplier mWaitingForAnimation;

        MiniOriginWindowInsetsAnimationListener(
                KeyboardVisibilityDelegate keyboardVisibilityDelegate,
                ViewGroup containerView,
                ObservableSupplierImpl<Integer> translationSupplier,
                Runnable animationStartedSignal,
                BooleanSupplier waitingForAnimation) {
            mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
            mContainerView = containerView;
            mTranslationSupplier = translationSupplier;
            mContext = containerView.getContext();
            mAnimationStartedSignal = animationStartedSignal;
            mWaitingForAnimation = waitingForAnimation;
        }

        @Override
        public void onPrepare(WindowInsetsAnimationCompat animation) {}

        @Override
        public void onStart(WindowInsetsAnimationCompat animation, BoundsCompat bounds) {
            if (!mWaitingForAnimation.getAsBoolean()
                    || ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0)) {
                return;
            }

            mAnimationStartedSignal.run();
            // Prevent clipping so that the mini origin bar can draw in bounds allocated for the
            // keyboard; we will prevent overlap by syncing our translation to its movement in
            // onProgress.
            ViewUtils.setAncestorsShouldClipChildren(mContainerView, false, View.NO_ID);
            ViewUtils.setAncestorsShouldClipToPadding(mContainerView, false, View.NO_ID);
            mAnimationInProgress = true;
            mFinalKeyboardHeight =
                    mKeyboardVisibilityDelegate.isKeyboardShowing(mContext, mContainerView)
                            ? bounds.getUpperBound().bottom
                            : 0;
        }

        @Override
        public void onProgress(
                WindowInsetsCompat windowInsetsCompat, List<WindowInsetsAnimationCompat> list) {
            if (!mAnimationInProgress) return;
            mTranslationSupplier.set(
                    mFinalKeyboardHeight
                            - windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom);
        }

        @Override
        public void onEnd(WindowInsetsAnimationCompat animation) {
            if (!mAnimationInProgress) return;
            mAnimationInProgress = false;
            ViewUtils.setAncestorsShouldClipChildren(mContainerView, true, ViewGroup.NO_ID);
            ViewUtils.setAncestorsShouldClipToPadding(mContainerView, true, ViewGroup.NO_ID);
            mTranslationSupplier.set(0);
        }
    }
}
