// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;

/**
 * Controller responsible for toggling the "mini origin" presentation of the browsing mode toolbar.
 * This state, which is not always active, has a reduced height and shows only the url bar with its
 * text at a reduced size.
 */
@NullMarked
public class MiniOriginBarController {
    private final LocationBar mLocationBar;
    private final ObservableSupplier<Boolean> mIsFormFieldFocusedSupplier;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Callback<Boolean> mIsFormFieldFocusedObserver;
    private final KeyboardVisibilityListener mKeyboardVisibilityObserver;
    private final Context mContext;
    private final ControlContainer mControlContainer;
    private boolean mShowMiniOriginBar;

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
            ControlContainer controlContainer) {
        mLocationBar = locationBar;
        mIsFormFieldFocusedSupplier = isFormFieldFocusedSupplier;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mContext = context;
        mControlContainer = controlContainer;

        mIsFormFieldFocusedObserver = (focused) -> updateMiniOriginBarState();
        mKeyboardVisibilityObserver = (showing) -> updateMiniOriginBarState();

        mIsFormFieldFocusedSupplier.addObserver(mIsFormFieldFocusedObserver);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityObserver);
    }

    private void updateMiniOriginBarState() {
        boolean isFormFieldFocused = mIsFormFieldFocusedSupplier.get();
        boolean isKeyboardVisible =
                mKeyboardVisibilityDelegate.isKeyboardShowing(
                        mContext, mControlContainer.getView());

        boolean showMiniOriginBar = isFormFieldFocused && isKeyboardVisible;
        if (showMiniOriginBar == mShowMiniOriginBar) return;

        mShowMiniOriginBar = showMiniOriginBar;
        mLocationBar.setShowOriginOnly(mShowMiniOriginBar);
    }

    public void destroy() {
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityObserver);
        mIsFormFieldFocusedSupplier.removeObserver(mIsFormFieldFocusedObserver);
    }
}
