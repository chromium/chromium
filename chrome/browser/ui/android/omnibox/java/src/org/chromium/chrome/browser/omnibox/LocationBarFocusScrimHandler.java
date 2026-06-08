// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

/** Handles showing and hiding a scrim when url bar focus changes. */
@NullMarked
public class LocationBarFocusScrimHandler {
    private static final String TAG = "ScrimHandler";

    /** The params used to control how the scrim behaves when shown for the omnibox. */
    private final PropertyModel mScrimModel;

    private final ScrimManager mScrimManager;

    /** Whether the scrim was shown on focus. */
    private boolean mScrimShown;

    /** The light color to use for the scrim on the NTP. */
    private final int mLightScrimColor;

    private final LocationBarDataProvider mLocationBarDataProvider;
    private final Context mContext;
    private final NonNullObservableSupplier<Integer> mTabStripHeightSupplier;
    private final Callback<Integer> mTabStripHeightChangeCallback;
    private final BottomControlsStacker mBottomControlsStacker;

    /**
     * @param scrimManager Coordinator responsible for showing and hiding the scrim view.
     * @param visibilityChangeCallback Callback used to obscure/unobscure tabs when the scrim is
     *     shown/hidden.
     * @param context Context for retrieving resources.
     * @param locationBarDataProvider Provider of location bar data, e.g. the NTP state.
     * @param clickDelegate Click handler for the scrim.
     * @param scrimTarget View that the scrim should be anchored to.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     */
    public LocationBarFocusScrimHandler(
            ScrimManager scrimManager,
            Callback<Boolean> visibilityChangeCallback,
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            Runnable clickDelegate,
            View scrimTarget,
            NonNullObservableSupplier<Integer> tabStripHeightSupplier,
            BottomControlsStacker bottomControlsStacker) {
        mScrimManager = scrimManager;
        mLocationBarDataProvider = locationBarDataProvider;
        mBottomControlsStacker = bottomControlsStacker;
        mContext = context;

        int topMargin = tabStripHeightSupplier.get();
        mLightScrimColor = context.getColor(R.color.omnibox_focused_fading_background_color_light);
        mScrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, scrimTarget)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                        .with(ScrimProperties.TOP_MARGIN, topMargin)
                        .with(ScrimProperties.CLICK_DELEGATE, clickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, visibilityChangeCallback)
                        .build();
        if (ChromeFeatureList.sDebugToolbarPositioning.isEnabled()) {
            Log.i(TAG, "Setting mScrimModel topMargin in constructor: %d", topMargin);
        }

        mTabStripHeightSupplier = tabStripHeightSupplier;
        mTabStripHeightChangeCallback =
                newHeight -> {
                    if (ChromeFeatureList.sDebugToolbarPositioning.isEnabled()) {
                        Log.i(
                                TAG,
                                "Setting mScrimModel topMargin from mTabStripHeightChangeCallback:"
                                        + " %d",
                                newHeight);
                    }
                    mScrimModel.set(ScrimProperties.TOP_MARGIN, newHeight);
                };
        mTabStripHeightSupplier.addSyncObserverAndPostIfNonNull(mTabStripHeightChangeCallback);
    }

    /** Compute and apply property updates needed for accurate visual representation of scrim. */
    public void updateScrimVisualState() {
        if (ChromeFeatureList.sOmniboxAutofocusOnIncognitoNtp.isEnabled()
                && mLocationBarDataProvider
                        .getNewTabPageDelegate()
                        .isIncognitoNewTabPageCurrentlyVisible()) {
            return;
        }

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        boolean useTransparentScrim =
                isTablet
                        && (OmniboxFeatures.isMultimodalInputEnabled(mContext)
                                || (OmniboxCapabilities.isDesktopPlatform()
                                        && OmniboxFeatures.sAndroidDesktopAimGate.isEnabled()));
        boolean useLightColor =
                !isTablet
                        && !mLocationBarDataProvider.isIncognitoBranded()
                        && !ColorUtils.inNightMode(mContext);
        @ColorInt Integer scrimColor = null;
        if (useTransparentScrim) {
            scrimColor = Color.TRANSPARENT;
        } else if (useLightColor) {
            scrimColor = mLightScrimColor;
        }
        mScrimModel.set(ScrimProperties.BACKGROUND_COLOR, scrimColor);
        mScrimModel.set(
                ScrimProperties.BOTTOM_MARGIN,
                mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_CHIN));
    }

    /** Controls the visibility of scrim overlay. */
    public void setVisibility(boolean shouldShow) {
        if (shouldShow == mScrimShown) return;

        if (shouldShow) {
            mScrimManager.showScrim(mScrimModel);
        } else {
            mScrimManager.hideScrim(mScrimModel, /* animate= */ true);
        }

        mScrimShown = shouldShow;
    }

    public void destroy() {
        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
    }

    PropertyModel getScrimModelForTesting() {
        return mScrimModel;
    }
}
