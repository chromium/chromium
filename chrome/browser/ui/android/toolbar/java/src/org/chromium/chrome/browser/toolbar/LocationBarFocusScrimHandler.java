// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

/** Handles showing and hiding a scrim when url bar focus changes. */
public class LocationBarFocusScrimHandler implements UrlFocusChangeListener {
    /** The params used to control how the scrim behaves when shown for the omnibox. */
    private PropertyModel mScrimModel;

    private final ScrimCoordinator mScrimCoordinator;

    /** Whether the scrim was shown on focus. */
    private boolean mScrimShown;

    /** The light color to use for the scrim on the NTP. */
    private int mLightScrimColor;

    private final LocationBarDataProvider mLocationBarDataProvider;
    private final Runnable mClickDelegate;
    private final Context mContext;
    private ObservableSupplier<Integer> mTabStripHeightSupplier;
    private Callback<Integer> mTabStripHeightChangeCallback;

    /**
     * @param scrimCoordinator Coordinator responsible for showing and hiding the scrim view.
     * @param visibilityChangeCallback Callback used to obscure/unobscure tabs when the scrim is
     *     shown/hidden.
     * @param context Context for retrieving resources.
     * @param locationBarDataProvider Provider of location bar data, e.g. the NTP state.
     * @param clickDelegate Click handler for the scrim.
     * @param scrimTarget View that the scrim should be anchored to.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     */
    public LocationBarFocusScrimHandler(
            ScrimCoordinator scrimCoordinator,
            Callback<Boolean> visibilityChangeCallback,
            Context context,
            LocationBarDataProvider locationBarDataProvider,
            Runnable clickDelegate,
            View scrimTarget,
            ObservableSupplier<Integer> tabStripHeightSupplier) {
        mScrimCoordinator = scrimCoordinator;
        mLocationBarDataProvider = locationBarDataProvider;
        mClickDelegate = clickDelegate;
        mContext = context;

        int topMargin = tabStripHeightSupplier.get() == null ? 0 : tabStripHeightSupplier.get();
        mLightScrimColor = context.getColor(R.color.omnibox_focused_fading_background_color_light);
        mScrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, scrimTarget)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                        .with(ScrimProperties.TOP_MARGIN, topMargin)
                        .with(ScrimProperties.CLICK_DELEGATE, mClickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, visibilityChangeCallback)
                        .with(ScrimProperties.BACKGROUND_COLOR, ScrimProperties.INVALID_COLOR)
                        .build();

        mTabStripHeightSupplier = tabStripHeightSupplier;
        mTabStripHeightChangeCallback =
                newHeight -> mScrimModel.set(ScrimProperties.TOP_MARGIN, newHeight);
        mTabStripHeightSupplier.addObserver(mTabStripHeightChangeCallback);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        boolean useLightColor =
                !isTablet
                        && !mLocationBarDataProvider.isIncognitoBranded()
                        && !ColorUtils.inNightMode(mContext);
        mScrimModel.set(
                ScrimProperties.BACKGROUND_COLOR,
                useLightColor ? mLightScrimColor : ScrimProperties.INVALID_COLOR);

        if (hasFocus && !showScrimAfterAnimationCompletes()) {
            mScrimCoordinator.showScrim(mScrimModel);
            mScrimShown = true;
        } else if (!hasFocus && mScrimShown) {
            mScrimCoordinator.hideScrim(true);
            mScrimShown = false;
        }
    }

    @Override
    public void onUrlAnimationFinished(boolean hasFocus) {
        if (hasFocus && showScrimAfterAnimationCompletes()) {
            mScrimCoordinator.showScrim(mScrimModel);
            mScrimShown = true;
        }
    }

    public void destroy() {
        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
    }

    /**
     * @return Whether the scrim should wait to be shown until after the omnibox is done
     *         animating.
     */
    private boolean showScrimAfterAnimationCompletes() {
        return mLocationBarDataProvider.getNewTabPageDelegate().isLocationBarShown();
    }

    PropertyModel getScrimModelForTesting() {
        return mScrimModel;
    }
}
