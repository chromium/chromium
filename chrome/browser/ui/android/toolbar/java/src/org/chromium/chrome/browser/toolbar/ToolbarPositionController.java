// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.Gravity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
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
    private final ControlContainer mControlContainer;
    private final BottomControlsStacker mBottomControlsStacker;
    private final ObservableSupplierImpl<Integer> mBrowserControlsOffsetSupplier;
    @LayerVisibility private int mLayerVisibility;
    private final BottomControlsLayer mBottomToolbarLayer;

    @ControlsPosition private int mCurrentPosition;

    /**
     * @param browserControlsSizer {@link BrowserControlsSizer}, used to manipulate position of the
     *     browser controls and relative heights of the top and bottom controls.
     * @param sharedPreferences SharedPreferences instance used to monitor user preference state.
     * @param isNtpShowingSupplier Supplier of the current state of the NTP. Must have a non-null
     *     value immediately available.
     * @param isOmniboxFocusedSupplier Supplier of the current omnibox focus state. Must have a
     *     non-null value immediately available.
     * @param controlContainer The control container for the current context.
     * @param bottomControlsStacker {@link BottomControlsStacker} used to harmonize the position of
     *     the bottom toolbar with other bottom-anchored UI.
     */
    public ToolbarPositionController(
            @NonNull BrowserControlsSizer browserControlsSizer,
            @NonNull SharedPreferences sharedPreferences,
            @NonNull ObservableSupplier<Boolean> isNtpShowingSupplier,
            @NonNull ObservableSupplier<Boolean> isOmniboxFocusedSupplier,
            @NonNull ControlContainer controlContainer,
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull ObservableSupplierImpl<Integer> browserControlsOffsetSupplier) {
        mBrowserControlsSizer = browserControlsSizer;
        mSharedPreferences = sharedPreferences;
        mIsNtpShowingSupplier = isNtpShowingSupplier;
        mIsOmniboxFocusedSupplier = isOmniboxFocusedSupplier;
        mControlContainer = controlContainer;
        mBottomControlsStacker = bottomControlsStacker;
        mBrowserControlsOffsetSupplier = browserControlsOffsetSupplier;
        mCurrentPosition = mBrowserControlsSizer.getControlsPosition();

        mIsNtpShowingSupplier.addObserver((showing) -> updateCurrentPosition());
        mIsOmniboxFocusedSupplier.addObserver((focused) -> updateCurrentPosition());
        sharedPreferences.registerOnSharedPreferenceChangeListener(this);

        mLayerVisibility = LayerVisibility.HIDDEN;
        mBottomToolbarLayer =
                new BottomControlsLayer() {
                    @Override
                    public int getType() {
                        return LayerType.BOTTOM_TOOLBAR;
                    }

                    @Override
                    public int getScrollBehavior() {
                        return LayerScrollBehavior.DEFAULT_SCROLL_OFF;
                    }

                    @Override
                    public int getHeight() {
                        return mControlContainer.getToolbarHeight();
                    }

                    @Override
                    public int getLayerVisibility() {
                        return mLayerVisibility;
                    }

                    @Override
                    public void onBrowserControlsOffsetUpdate(int layerYOffset) {
                        if (mLayerVisibility == LayerVisibility.VISIBLE) {
                            mBrowserControlsOffsetSupplier.set(layerYOffset);
                            mControlContainer.getView().setTranslationY(layerYOffset);
                        }
                    }
                };
        mBottomControlsStacker.addLayer(mBottomToolbarLayer);
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
        if (ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED.equals(key)) {
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
        int controlContainerHeight = mControlContainer.getToolbarHeight();

        if (newControlsPosition == ControlsPosition.TOP) {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() + controlContainerHeight;
            mLayerVisibility = LayerVisibility.HIDDEN;
            mControlContainer.getView().setTranslationY(0);
        } else {
            newTopHeight = mBrowserControlsSizer.getTopControlsHeight() - controlContainerHeight;
            mLayerVisibility = LayerVisibility.VISIBLE;
        }

        mBottomControlsStacker.requestLayerUpdate(false);

        mCurrentPosition = newControlsPosition;
        mBrowserControlsSizer.setControlsPosition(
                mCurrentPosition,
                newTopHeight,
                mBrowserControlsSizer.getTopControlsMinHeight(),
                mBottomControlsStacker.getTotalHeight(),
                mBottomControlsStacker.getTotalMinHeight());

        CoordinatorLayout.LayoutParams layoutParams = mControlContainer.mutateLayoutParams();
        int verticalGravity =
                mCurrentPosition == ControlsPosition.TOP ? Gravity.TOP : Gravity.BOTTOM;
        layoutParams.gravity = Gravity.START | verticalGravity;
    }
}
