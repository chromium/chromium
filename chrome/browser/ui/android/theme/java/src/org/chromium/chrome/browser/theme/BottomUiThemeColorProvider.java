// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/**
 * Theme color provider that provides theme color and tinting for bottom controls. The coloring
 * provided depends on the position of the control container. The color and tint follow:
 *
 * <p>1. A pair of hardcoded bg + tint combos (one for incognito, one otherwise) when the toolbar is
 * top-anchored
 *
 * <p>2. The color and tint of the toolbar's color provider when the toolbar is bottom-anchored.
 * This allows other bottom controls using this class to match the toolbar's color when it's
 * visually adjacent to them.
 *
 * <p>3. When AndroidBottomBar is enabled, the behavior of 1 and 2 is ignored. The background color
 * is determined by the BottomControlsStacker, which is the source of truth for the theme color of
 * the bottom controls. The tints continue to use the hardcoded combos.
 */
@NullMarked
public class BottomUiThemeColorProvider extends ThemeColorProvider
        implements BrowserControlsStateProvider.Observer,
                IncognitoStateObserver,
                ThemeColorObserver,
                TintObserver {

    private final ThemeColorProvider mToolbarThemeColorProvider;
    private final @ColorInt int mPrimaryBackgroundColorWithTopToolbar;
    private final @ColorInt int mIncognitoBackgroundColorWithTopToolbar;
    private final ColorStateList mPrimaryTintWithTopToolbar;
    private final ColorStateList mIncognitoTintWithTopToolbar;
    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BottomControlsStacker mBottomControlsStacker;
    private final IncognitoStateProvider mIncognitoStateProvider;
    // This flag never changes during runtime so we can cache it.
    private final boolean mIsBottomBarEnabled;
    private @ControlsPosition int mControlsPosition;
    private boolean mIncognito;

    /**
     * @param toolbarThemeColorProvider Theme color provider for the toolbar contained in the
     *     control container, which can be either bottom- or top-anchored.
     * @param browserControlsStateProvider Provider of the state of the browser controls.
     * @param bottomControlsStacker BrowserControlsStacker instance.
     * @param incognitoStateProvider Provided of current incognito state.
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public BottomUiThemeColorProvider(
            ThemeColorProvider toolbarThemeColorProvider,
            BrowserControlsStateProvider browserControlsStateProvider,
            BottomControlsStacker bottomControlsStacker,
            IncognitoStateProvider incognitoStateProvider,
            Context context) {
        super(context);
        mContext = context;
        mToolbarThemeColorProvider = toolbarThemeColorProvider;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBottomControlsStacker = bottomControlsStacker;
        mIncognitoStateProvider = incognitoStateProvider;
        mIsBottomBarEnabled = BottomBarConfigUtils.isBottomBarEnabled(context);
        mControlsPosition = browserControlsStateProvider.getControlsPosition();
        mPrimaryBackgroundColorWithTopToolbar = SemanticColorUtils.getColorSurface(context);
        mIncognitoBackgroundColorWithTopToolbar = context.getColor(R.color.tab_strip_bg_incognito);
        mPrimaryTintWithTopToolbar =
                mContext.getColorStateList(R.color.default_icon_color_tint_list);
        mIncognitoTintWithTopToolbar =
                mContext.getColorStateList(R.color.default_icon_color_light_tint_list);

        mToolbarThemeColorProvider.addThemeColorObserver(this);
        mToolbarThemeColorProvider.addTintObserver(this);
        mBrowserControlsStateProvider.addObserver(this);
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    @Override
    public void destroy() {
        mToolbarThemeColorProvider.removeThemeColorObserver(this);
        mToolbarThemeColorProvider.removeTintObserver(this);
        mBrowserControlsStateProvider.removeObserver(this);
        mIncognitoStateProvider.removeObserver(this);
    }

    // BrowserControlsStateProvider.Observer implementation.
    @Override
    public void onControlsPositionChanged(int controlsPosition) {
        mControlsPosition = controlsPosition;
        updateColorAndTint(false);
    }

    @Override
    public void onBottomControlsBackgroundColorChanged(@ColorInt int backgroundColor) {
        // When the bottom bar is enabled, BottomControlsStacker becomes the source of truth for the
        // theme color of the bottom controls so we need to listen for its background color changes.
        //
        // When the bottom bar is disabled BottomUiThemeColorProvider drives updates into the
        // BottomControlsStacker. See updateColorAndTint().
        if (mIsBottomBarEnabled) {
            updateColorAndTint(false);
        }
    }

    // IncognitoStateObserver implementation.
    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        mIncognito = isIncognito;
        updateColorAndTint(false);
    }

    // ThemeColorObserver implementation.
    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        updateColorAndTint(shouldAnimate);
    }

    // TintObserver implementation.
    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        updateColorAndTint(false);
    }

    private void updateColorAndTint(boolean animate) {
        if (mIsBottomBarEnabled) {
            updatePrimaryColor(mBottomControlsStacker.getBackgroundColor(), animate);
            // Here we assume that while the BottomControlsStacker background color might be
            // slightly different from the toolbar background color, the tints should follow the
            // defaults for the current theme. If this assumption doesn't hold,
            // BottomControlsStacker should provide a separate API to return the correct tint.
            ColorStateList tint = getTintForTopAnchoredToolbar();
            updateTint(tint, tint, getBrandedColorSchemeForTopAnchoredToolbar());
        } else if (mControlsPosition == ControlsPosition.TOP) {
            updatePrimaryColor(getColorForTopAnchoredToolbar(), animate);
            ColorStateList tint = getTintForTopAnchoredToolbar();
            updateTint(tint, tint, getBrandedColorSchemeForTopAnchoredToolbar());
        } else {
            updatePrimaryColor(mToolbarThemeColorProvider.getThemeColor(), animate);
            updateTint(
                    mToolbarThemeColorProvider.getTint(),
                    mToolbarThemeColorProvider.getActivityFocusTint(),
                    mToolbarThemeColorProvider.getBrandedColorScheme());
        }
        if (!mIsBottomBarEnabled) {
            mBottomControlsStacker.notifyBackgroundColor(getThemeColor());
        }
    }

    private ColorStateList getTintForTopAnchoredToolbar() {
        return mIncognito ? mIncognitoTintWithTopToolbar : mPrimaryTintWithTopToolbar;
    }

    private int getColorForTopAnchoredToolbar() {
        return mIncognito
                ? mIncognitoBackgroundColorWithTopToolbar
                : mPrimaryBackgroundColorWithTopToolbar;
    }

    private @BrandedColorScheme int getBrandedColorSchemeForTopAnchoredToolbar() {
        return mIncognito ? BrandedColorScheme.INCOGNITO : BrandedColorScheme.APP_DEFAULT;
    }
}
