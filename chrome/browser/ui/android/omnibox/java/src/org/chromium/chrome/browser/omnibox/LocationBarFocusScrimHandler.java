// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Color;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Handles showing and hiding a scrim when url bar focus changes. */
@NullMarked
public class LocationBarFocusScrimHandler {
    private static final String TAG = "ScrimHandler";

    /** The possible visual states of the omnibox focus scrim. */
    @IntDef({ScrimState.DEFAULT, ScrimState.LIGHT_COLOR, ScrimState.TRANSPARENT, ScrimState.OPAQUE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ScrimState {
        /** The ScrimManager's default scrim color. */
        int DEFAULT = 0;

        /** A light, translucent color, used on phones in light mode outside of incognito. */
        int LIGHT_COLOR = 1;

        /** Fully transparent; used on tablets where suggestions appear in a popover. */
        int TRANSPARENT = 2;

        /** Fully opaque, obscuring the content beneath it. */
        int OPAQUE = 3;
    }

    /** The params used to control how the scrim behaves when shown for the omnibox. */
    private final PropertyModel mScrimModel;

    private final ScrimManager mScrimManager;

    /** Whether the scrim was shown on focus. */
    private boolean mScrimShown;

    /** The light color to use for the scrim on the NTP. */
    private final int mLightScrimColor;

    /** Whether the scrim should be fully opaque, obscuring the content beneath it. */
    private boolean mUseOpaqueScrim;

    private final LocationBarDataProvider mLocationBarDataProvider;
    private final Context mContext;
    private final NonNullObservableSupplier<Integer> mTabStripHeightSupplier;
    private final Callback<Integer> mTabStripHeightChangeCallback;
    private final BottomControlsStacker mBottomControlsStacker;
    private final SettableNonNullObservableSupplier<Boolean> mScrimVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);

    /**
     * @param scrimManager Coordinator responsible for showing and hiding the scrim view.
     * @param visibilityChangeCallback Callback used to obscure/unobscure tabs when the scrim is
     *     shown/hidden.
     * @param context Context for retrieving resources.
     * @param locationBarDataProvider Provider of location bar data, e.g. the NTP state.
     * @param clickDelegate Click handler for the scrim.
     * @param scrimTarget View that the scrim should be anchored to.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param bottomControlsStacker Controller managing positioning of bottom controls.
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
        Callback<Boolean> visibilityChangeCallbackWrapper =
                visible -> {
                    visibilityChangeCallback.onResult(visible);
                    mScrimVisibilitySupplier.set(visible);
                };

        int topMargin = tabStripHeightSupplier.get();
        mLightScrimColor = context.getColor(R.color.omnibox_focused_fading_background_color_light);
        mScrimModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, scrimTarget)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                        .with(ScrimProperties.TOP_MARGIN, topMargin)
                        .with(ScrimProperties.CLICK_DELEGATE, clickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, visibilityChangeCallbackWrapper)
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
        @ColorInt
        Integer scrimColor =
                switch (computeScrimState()) {
                    case ScrimState.OPAQUE ->
                            OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                                    mContext,
                                    mLocationBarDataProvider.isIncognitoBranded()
                                            ? BrandedColorScheme.INCOGNITO
                                            : BrandedColorScheme.APP_DEFAULT);
                    case ScrimState.TRANSPARENT -> Color.TRANSPARENT;
                    case ScrimState.LIGHT_COLOR -> mLightScrimColor;
                    // A null color lets the ScrimManager apply its default scrim color.
                    case ScrimState.DEFAULT -> null;
                    default -> throw new IllegalStateException("Unhandled scrim state");
                };
        mScrimModel.set(ScrimProperties.BACKGROUND_COLOR, scrimColor);

        // With the toolbar at the bottom, stop the scrim at the top of the toolbar so it doesn't
        // tint the bottom controls beneath it, e.g. the bottom chin (crbug.com/452336254). With
        // the toolbar at the top, the bottom inset is covered by the keyboard or the suggestions
        // list, and cropping the scrim would only reveal an unscrimmed band above the keyboard
        // when the scrim's container is resized to end at the keyboard.
        int bottomMargin =
                mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_TOOLBAR)
                        ? mBottomControlsStacker.getHeightFromLayerToBottom(
                                LayerType.BOTTOM_TOOLBAR)
                        : 0;
        mScrimModel.set(ScrimProperties.BOTTOM_MARGIN, bottomMargin);
    }

    /** Returns the {@link ScrimState} the scrim should currently be drawn in. */
    private @ScrimState int computeScrimState() {
        if (mUseOpaqueScrim) return ScrimState.OPAQUE;

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        if (isTablet
                && (OmniboxFeatures.isMultimodalInputEnabled(mContext)
                        || OmniboxCapabilities.isDesktopPlatform())) {
            return ScrimState.TRANSPARENT;
        }

        if (!isTablet
                && !mLocationBarDataProvider.isIncognitoBranded()
                && !ColorUtils.inNightMode(mContext)) {
            return ScrimState.LIGHT_COLOR;
        }

        return ScrimState.DEFAULT;
    }

    /**
     * Sets whether the scrim should be drawn with an opaque color, fully obscuring the content
     * beneath it. Updates the scrim visual state if the value changed.
     */
    public void setUseOpaqueScrim(boolean useOpaqueScrim) {
        if (mUseOpaqueScrim == useOpaqueScrim) return;
        mUseOpaqueScrim = useOpaqueScrim;
        updateScrimVisualState();
    }

    /** Controls the visibility of scrim overlay. */
    public void setVisibility(boolean shouldShow) {
        setVisibility(shouldShow, /* animate= */ true);
    }

    public void setVisibility(boolean shouldShow, boolean animate) {
        if (shouldShow == mScrimShown) return;

        if (shouldShow) {
            mScrimManager.showScrim(mScrimModel);
        } else {
            mScrimManager.hideScrim(mScrimModel, animate);
        }

        mScrimShown = shouldShow;
    }

    /** Returns whether the scrim is currently shown. */
    public boolean isScrimShown() {
        return mScrimVisibilitySupplier.get();
    }

    /** Returns a supplier for whether the scrim is currently shown. */
    public NonNullObservableSupplier<Boolean> getScrimVisibilitySupplier() {
        return mScrimVisibilitySupplier;
    }

    public void destroy() {
        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
    }

    PropertyModel getScrimModelForTesting() {
        return mScrimModel;
    }
}
