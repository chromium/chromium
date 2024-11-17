// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.theme;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;
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
 */
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
    private final IncognitoStateProvider mIncognitoStateProvider;
    private @ControlsPosition int mControlsPosition;
    private boolean mIncognito;

    /**
     * @param toolbarThemeColorProvider Theme color provider for the toolbar contained in the
     *     control container, which can be either bottom- or top-anchored.
     * @param browserControlsStateProvider Provider of the state of the browser controls.
     * @param incognitoStateProvider Provided of current incognito state.
     * @param context The {@link Context} that is used to retrieve color related resources.
     */
    public BottomUiThemeColorProvider(
            @NonNull ThemeColorProvider toolbarThemeColorProvider,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull Context context) {
        super(context);
        mContext = context;
        mToolbarThemeColorProvider = toolbarThemeColorProvider;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mIncognitoStateProvider = incognitoStateProvider;
        mControlsPosition = browserControlsStateProvider.getControlsPosition();
        mPrimaryBackgroundColorWithTopToolbar = SemanticColorUtils.getDialogBgColor(context);
        mIncognitoBackgroundColorWithTopToolbar =
                context.getColor(R.color.dialog_bg_color_dark_baseline);

        mPrimaryTintWithTopToolbar =
                ContextCompat.getColorStateList(mContext, R.color.default_icon_color_tint_list);
        mIncognitoTintWithTopToolbar =
                ContextCompat.getColorStateList(
                        mContext, R.color.default_icon_color_light_tint_list);

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
            ColorStateList tint, ColorStateList activityFocusTint, int brandedColorScheme) {
        updateColorAndTint(false);
    }

    private void updateColorAndTint(boolean animate) {
        if (mControlsPosition == ControlsPosition.TOP) {
            updatePrimaryColor(getColorForTopAnchoredToolbar(), animate);
            updateTint(
                    getTintForTopAnchoredToolbar(),
                    getTintForTopAnchoredToolbar(),
                    BrandedColorScheme.APP_DEFAULT);
        } else {
            updatePrimaryColor(mToolbarThemeColorProvider.getThemeColor(), animate);
            updateTint(
                    mToolbarThemeColorProvider.getTint(),
                    mToolbarThemeColorProvider.getActivityFocusTint(),
                    mToolbarThemeColorProvider.getBrandedColorScheme());
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
}
