// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;

/**
 * Theme color provider for the toolbar. It listens to {@link TopUiThemeColorProvider} by default.
 * If {@link BottomBarConfigUtils#isBottomBarEnabled(Context)} is true, it switches to {@link
 * BottomUiThemeColorProvider} when the controls position is {@link ControlsPosition#BOTTOM}.
 */
@NullMarked
public class ToolbarThemeColorProvider extends ThemeColorProvider
        implements BrowserControlsStateProvider.Observer, ThemeColorObserver, TintObserver {
    private final TopUiThemeColorProvider mTopUiThemeColorProvider;
    private final BottomUiThemeColorProvider mBottomUiThemeColorProvider;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final boolean mIsBottomBarEnabled;
    private @ControlsPosition int mControlsPosition;
    private @Nullable ThemeColorProvider mActiveProvider;

    /**
     * @param context The {@link Context} to check if bottom bar is enabled.
     * @param topUiThemeColorProvider The default provider.
     * @param bottomUiThemeColorProvider The provider to use when bottom bar is enabled and position
     *     is BOTTOM.
     * @param browserControlsStateProvider Provider of the browser controls state to listen for
     *     position changes.
     */
    public ToolbarThemeColorProvider(
            Context context,
            TopUiThemeColorProvider topUiThemeColorProvider,
            BottomUiThemeColorProvider bottomUiThemeColorProvider,
            BrowserControlsStateProvider browserControlsStateProvider) {
        super(context);
        mTopUiThemeColorProvider = topUiThemeColorProvider;
        mBottomUiThemeColorProvider = bottomUiThemeColorProvider;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mIsBottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(context);
        mControlsPosition = browserControlsStateProvider.getControlsPosition();

        mBrowserControlsStateProvider.addObserver(this);
        updateActiveProvider(/* animate= */ false);
    }

    /**
     * @param tab The {@link Tab} on which the theme color is used.
     * @param fallbackColor The fallback color to use if the default color is used or there is no
     *     current tab.
     * @return Theme color or the given fallback color if the default color is used or there is no
     *     current tab.
     */
    public @ColorInt int getThemeColorOrFallback(@Nullable Tab tab, @ColorInt int fallbackColor) {
        if (mActiveProvider == mTopUiThemeColorProvider) {
            return mTopUiThemeColorProvider.getThemeColorOrFallback(tab, fallbackColor);
        }
        return mBottomUiThemeColorProvider.getThemeColor();
    }

    /**
     * @param tab The {@link Tab} on which the toolbar background color is used.
     * @return Returns the toolbar background color.
     */
    public @ColorInt int getToolbarBackgroundColor(Tab tab) {
        if (mActiveProvider == mTopUiThemeColorProvider) {
            return mTopUiThemeColorProvider.getToolbarBackgroundColor(tab);
        }
        return mBottomUiThemeColorProvider.getThemeColor();
    }

    @Override
    public void destroy() {
        super.destroy();
        mBrowserControlsStateProvider.removeObserver(this);
        if (mActiveProvider != null) {
            mActiveProvider.removeThemeColorObserver(this);
            mActiveProvider.removeTintObserver(this);
        }
    }

    @Override
    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
        if (mControlsPosition == controlsPosition) return;
        mControlsPosition = controlsPosition;
        updateActiveProvider(/* animate= */ true);
    }

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        updatePrimaryColor(color, shouldAnimate);
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        updateTint(tint, activityFocusTint, brandedColorScheme);
    }

    private void updateActiveProvider(boolean animate) {
        ThemeColorProvider newProvider;
        if (mIsBottomBarEnabled && mControlsPosition == ControlsPosition.BOTTOM) {
            newProvider = mBottomUiThemeColorProvider;
        } else {
            newProvider = mTopUiThemeColorProvider;
        }

        if (newProvider != mActiveProvider) {
            if (mActiveProvider != null) {
                mActiveProvider.removeThemeColorObserver(this);
                mActiveProvider.removeTintObserver(this);
            }
            mActiveProvider = newProvider;
            mActiveProvider.addThemeColorObserver(this);
            mActiveProvider.addTintObserver(this);

            updatePrimaryColor(mActiveProvider.getThemeColor(), animate);
            updateTint(
                    mActiveProvider.getTint(),
                    mActiveProvider.getActivityFocusTint(),
                    mActiveProvider.getBrandedColorScheme());
        }
    }
}
