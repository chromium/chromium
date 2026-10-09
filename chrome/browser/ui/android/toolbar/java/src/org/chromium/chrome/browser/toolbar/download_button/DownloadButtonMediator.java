// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.Context;
import android.content.res.ColorStateList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator controlling the property model of the download toolbar button. */
@NullMarked
class DownloadButtonMediator {
    private final PropertyModel mModel;
    private final ColorStateList mAccentTint;
    private final ColorStateList mIncognitoAccentTint;
    private boolean mHasSpaceToShow;
    private DownloadToolbarButtonState mState = DownloadToolbarButtonState.HIDDEN;
    private @Nullable ColorStateList mDefaultTint;
    private ColorStateList mActiveTint;

    /**
     * Creates a new {@link DownloadButtonMediator}.
     *
     * @param context The Android context.
     * @param model The {@link PropertyModel} for the download button.
     * @param themeColorProvider The provider for theme colors.
     * @param onClickRunnable Callback to invoke when the button is clicked.
     */
    DownloadButtonMediator(
            Context context,
            PropertyModel model,
            ThemeColorProvider themeColorProvider,
            Runnable onClickRunnable) {
        mModel = model;
        // Same accent as other "on" toolbar icons, e.g. the filled bookmark star.
        mAccentTint = context.getColorStateList(R.color.default_icon_color_accent1_tint_list);
        mIncognitoAccentTint = context.getColorStateList(R.color.default_icon_color_blue_light);
        mActiveTint = mAccentTint;
        mModel.set(DownloadButtonProperties.ON_CLICK, v -> onClickRunnable.run());
        onTintChanged(
                themeColorProvider.getActivityFocusTint(),
                themeColorProvider.getBrandedColorScheme());
    }

    /** Applies a new download state to the model. */
    void setState(DownloadToolbarButtonState state) {
        mState = state;
        mModel.set(DownloadButtonProperties.SHOULD_SHOW, state.shouldShow);
        updateButtonVisibility();
        updateTint();
    }

    /**
     * Updates the default and active tints from the current theme state.
     *
     * @param activityFocusTint The toolbar's icon tint, used while the state is inactive.
     * @param brandedColorScheme The current {@link BrandedColorScheme}, used to pick the active
     *     accent tint.
     */
    void onTintChanged(
            @Nullable ColorStateList activityFocusTint,
            @BrandedColorScheme int brandedColorScheme) {
        mDefaultTint = activityFocusTint;
        mActiveTint =
                brandedColorScheme == BrandedColorScheme.INCOGNITO
                        ? mIncognitoAccentTint
                        : mAccentTint;
        updateTint();
    }

    boolean shouldShow() {
        return mModel.get(DownloadButtonProperties.SHOULD_SHOW);
    }

    boolean isVisible() {
        return mModel.get(DownloadButtonProperties.IS_VISIBLE);
    }

    boolean hasSpaceToShow() {
        return mHasSpaceToShow;
    }

    void setHasSpaceToShow(boolean hasSpaceToShow) {
        mHasSpaceToShow = hasSpaceToShow;
        updateButtonVisibility();
    }

    /** Updates the button visibility on the model based on shouldShow and space availability. */
    private void updateButtonVisibility() {
        boolean isVisible = mModel.get(DownloadButtonProperties.SHOULD_SHOW) && mHasSpaceToShow;
        mModel.set(DownloadButtonProperties.IS_VISIBLE, isVisible);
    }

    private void updateTint() {
        mModel.set(DownloadButtonProperties.TINT, mState.isActive ? mActiveTint : mDefaultTint);
    }
}
