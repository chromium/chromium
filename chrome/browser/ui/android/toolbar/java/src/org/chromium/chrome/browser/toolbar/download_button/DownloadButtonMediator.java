// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.download_button;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.download_button.DownloadProgressRingDrawable.RingStatus;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator controlling the property model of the download toolbar button. */
@NullMarked
class DownloadButtonMediator implements ThemeColorProvider.ThemeColorObserver {
    private final PropertyModel mModel;
    private final ThemeColorProvider mThemeColorProvider;
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
        mThemeColorProvider = themeColorProvider;
        // Same accent as other "on" toolbar icons, e.g. the filled bookmark star.
        mAccentTint = context.getColorStateList(R.color.default_icon_color_accent1_tint_list);
        mIncognitoAccentTint = context.getColorStateList(R.color.default_icon_color_blue_light);
        mActiveTint = mAccentTint;
        mModel.set(DownloadButtonProperties.ON_CLICK, v -> onClickRunnable.run());
        updateIconAndRing();
        mThemeColorProvider.addThemeColorObserver(this);
        onThemeColorChanged(themeColorProvider.getThemeColor(), /* shouldAnimate= */ false);
        onTintChanged(
                themeColorProvider.getActivityFocusTint(),
                themeColorProvider.getBrandedColorScheme());
    }

    /** Stops observing the theme color provider. */
    void destroy() {
        mThemeColorProvider.removeThemeColorObserver(this);
    }

    /** Applies a new download state to the model. */
    void setState(DownloadToolbarButtonState state) {
        mState = state;
        mModel.set(DownloadButtonProperties.SHOULD_SHOW, state.shouldShow);
        updateButtonVisibility();
        updateTint();
        updateIconAndRing();
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

    // ThemeColorProvider.ThemeColorObserver implementation:

    @Override
    public void onThemeColorChanged(@ColorInt int color, boolean shouldAnimate) {
        // Fill the badge with the toolbar color so it reads as a cutout over the ring.
        mModel.set(DownloadButtonProperties.BADGE_BACKGROUND_COLOR, color);
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

    /** Mirrors desktop's {@code DownloadToolbarUIController::UpdateIcon}. */
    private void updateIconAndRing() {
        boolean isComplete = mState.iconState == IconState.COMPLETE;
        // Once complete, show the arrow and tray; otherwise show a standalone arrow.
        mModel.set(
                DownloadButtonProperties.ICON_RES,
                isComplete ? R.drawable.ic_download : R.drawable.ic_download_in_progress);

        @RingStatus int ringStatus;
        if (isComplete || mState.downloadCount == 0) {
            ringStatus = RingStatus.IDLE;
        } else if (!mState.progressCertain) {
            ringStatus = RingStatus.SCANNING;
        } else {
            ringStatus = RingStatus.DOWNLOADING;
        }
        // Set progress first so a status change to DOWNLOADING never draws a stale percentage.
        mModel.set(DownloadButtonProperties.PROGRESS_PERCENT, mState.progressPercent);
        mModel.set(DownloadButtonProperties.RING_STATUS, ringStatus);

        // Like desktop, the count is only shown while active, e.g. not when everything is paused.
        mModel.set(
                DownloadButtonProperties.BADGE_COUNT,
                mState.isActive && mState.downloadCount >= 2 ? mState.downloadCount : 0);
    }
}
