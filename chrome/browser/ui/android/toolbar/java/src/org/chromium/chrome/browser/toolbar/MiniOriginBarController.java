// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.Observer;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;

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
    private boolean mShowMiniOriginBar;
    private int mDefaultLocationBarGravity;
    private boolean mOriginBarClickedInSession;
    private TouchEventObserver mTouchEventObserver;

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
            BrowserControlsSizer browserControlsSizer) {
        mLocationBar = locationBar;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mContext = context;
        mControlContainer = controlContainer;
        mSuppressToolbarSceneLayerSupplier = suppressToolbarSceneLayerSupplier;
        mBrowserControlsSizer = browserControlsSizer;
        mDefaultLocationBarGravity =
                ((FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams())
                        .gravity;
        mBrowserControlsSizer.addObserver(this);

        mIsFormFieldFocusedObserver = (focused) -> updateMiniOriginBarState();
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

        mShowMiniOriginBar = showMiniOriginBar;
        mLocationBar.setShowOriginOnly(mShowMiniOriginBar);
        mLocationBar.setUrlBarUsesSmallText(mShowMiniOriginBar);
        mSuppressToolbarSceneLayerSupplier.set(mShowMiniOriginBar);
        mControlContainer.toggleLocationBarOnlyMode(mShowMiniOriginBar);

        int newControlContainerHeight =
                showMiniOriginBar
                        ? mContext.getResources()
                                .getDimensionPixelSize(R.dimen.mini_origin_bar_height)
                        : LayoutParams.WRAP_CONTENT;
        mControlContainer.mutateLayoutParams().height = newControlContainerHeight;

        var locationBarLayoutParams =
                ((FrameLayout.LayoutParams) mLocationBar.getContainerView().getLayoutParams());
        locationBarLayoutParams.gravity =
                showMiniOriginBar ? Gravity.CENTER : mDefaultLocationBarGravity;
        mLocationBar.getContainerView().setLayoutParams(locationBarLayoutParams);
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
}
