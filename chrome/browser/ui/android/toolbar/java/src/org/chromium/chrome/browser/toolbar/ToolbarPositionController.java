// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.ui.base.DeviceFormFactor;

/** Class responsible for managing the position (top, bottom) of the browsing mode toolbar. */
public class ToolbarPositionController implements OnSharedPreferenceChangeListener {

    private final BrowserControlsSizer mBrowserControlsSizer;
    private final SharedPreferences mSharedPreferences;
    private final ObservableSupplier<Boolean> mIsNtpShowingSupplier;
    private final ObservableSupplier<Boolean> mIsOmniboxFocusedSupplier;

    private final int mControlContainerHeight;
    @ControlsPosition private int mCurrentPosition;

    /**
     * @param browserControlsSizer {@link BrowserControlsSizer}, used to manipulate position of the
     *     browser controls and relative heights of the top and bottom controls.
     * @param sharedPreferences SharedPreferences instance used to monitor user preference state.
     * @param isNtpShowingSupplier Supplier of the current state of the NTP. Must have a non-null
     *     value immediately available.
     * @param isOmniboxFocusedSupplier Supplier of the current omnibox focus state. Must have a
     *     non-null value immediately available.
     * @param controlContainerHeight The total height of the control container.
     */
    public ToolbarPositionController(
            @NonNull BrowserControlsSizer browserControlsSizer,
            @NonNull SharedPreferences sharedPreferences,
            @NonNull ObservableSupplier<Boolean> isNtpShowingSupplier,
            @NonNull ObservableSupplier<Boolean> isOmniboxFocusedSupplier,
            int controlContainerHeight) {
        mBrowserControlsSizer = browserControlsSizer;
        mSharedPreferences = sharedPreferences;
        mIsNtpShowingSupplier = isNtpShowingSupplier;
        mIsOmniboxFocusedSupplier = isOmniboxFocusedSupplier;
        mControlContainerHeight = controlContainerHeight;
        mCurrentPosition = mBrowserControlsSizer.getControlsPosition();

        mIsNtpShowingSupplier.addObserver((showing) -> updateCurrentPosition());
        mIsOmniboxFocusedSupplier.addObserver((focused) -> updateCurrentPosition());
        sharedPreferences.registerOnSharedPreferenceChangeListener(this);
        updateCurrentPosition();
    }

    /**
     * Returns whether the given {context, device, cct-ness} combo is eligible for toolbar position
     * customization.
     */
    public static boolean isToolbarPositionCustomizationEnabled(
            Context context, boolean isCustomTab) {
        return !isCustomTab
                && ChromeFeatureList.sAndroidBottomToolbar.isEnabled()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                // Some emulators erroneously report that they have a hinge sensor (and thus are
                // foldables). To make the feature testable on these "devices", skip the foldable
                // check for debug builds.
                && (!BuildInfo.getInstance().isFoldable || BuildInfo.isDebugApp());
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (key.equals(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED)) {
            updateCurrentPosition();
        }
    }

    private void updateCurrentPosition() {
        boolean ntpShowing = mIsNtpShowingSupplier.get();
        boolean isOmniboxFocused = mIsOmniboxFocusedSupplier.get();
        boolean doesUserPreferTopToolbar =
                mSharedPreferences.getBoolean(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        @ControlsPosition int newControlsPosition;
        if (ntpShowing || isOmniboxFocused || doesUserPreferTopToolbar) {
            newControlsPosition = ControlsPosition.TOP;
        } else {
            newControlsPosition = ControlsPosition.BOTTOM;
        }
        if (newControlsPosition == mCurrentPosition) return;

        int newTopHeight;
        int newBottomHeight;

        if (newControlsPosition == ControlsPosition.TOP) {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() + mControlContainerHeight;
            newBottomHeight =
                    mBrowserControlsSizer.getBottomControlsHeight() - mControlContainerHeight;
        } else {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() - mControlContainerHeight;
            newBottomHeight =
                    mBrowserControlsSizer.getBottomControlsHeight() + mControlContainerHeight;
        }

        mCurrentPosition = newControlsPosition;
        mBrowserControlsSizer.setControlsPosition(
                mCurrentPosition,
                newTopHeight,
                mBrowserControlsSizer.getTopControlsMinHeight(),
                newBottomHeight,
                mBrowserControlsSizer.getBottomControlsMinHeight());
    }
}
