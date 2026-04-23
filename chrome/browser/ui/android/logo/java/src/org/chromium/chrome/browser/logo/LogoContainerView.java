// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.widget.LoadingView;

/** Container view for the logo and loading view. */
@NullMarked
public class LogoContainerView extends FrameLayout {
    private LogoView mLogoView;
    private LoadingView mLoadingView;

    public LogoContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public void updateLogo(Logo logo) {
        boolean shouldUpdateLogo = true;

        // If no logo is provided, try to show the default logo first.
        if (logo == null) {
            shouldUpdateLogo = !maybeShowDefaultLogoDrawable();
        }

        // If a specific logo was provided, update the logo view.
        // If no logo was provided (logo is null) and default logo is not available, clear the logo
        // via updateLogo(null).
        if (shouldUpdateLogo) {
            mLogoView.updateLogo(logo);
        }

        // If a specific logo was provided, hide loading ui.
        // Note: maybeShowDefaultLogoDrawable() handles hiding loading UI for the null case.
        if (logo != null) {
            hideLoadingUi();
        }
    }

    public void endAnimationsForTesting() {
        mLogoView.endAnimationsForTesting(); // IN-TEST
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLogoView = findViewById(R.id.search_provider_logo);
        mLoadingView = findViewById(R.id.loading_view);
    }

    void showSearchProviderInitialView() {
        boolean isLogoAvailable = maybeShowDefaultLogoDrawable();
        if (!isLogoAvailable) {
            showLoadingView();
        }
    }

    void playAnimatedLogo(Object animatedLogo) {
        mLoadingView.hideLoadingUi();
        mLogoView.playAnimatedLogo(animatedLogo);
    }

    void setDefaultGoogleLogoDrawable(Drawable defaultGoogleLogoDrawable) {
        mLogoView.setDefaultGoogleLogoDrawable(defaultGoogleLogoDrawable);
    }

    /**
     * Shows the default search engine logo and hide loading UI if available.
     *
     * @return Whether the default search engine logo drawable is available.
     */
    boolean maybeShowDefaultLogoDrawable() {
        boolean shown = mLogoView.maybeShowDefaultLogoDrawable();
        if (shown) {
            mLoadingView.hideLoadingUi();
        }
        return shown;
    }

    void showLoadingView() {
        mLogoView.clearLogo();
        mLoadingView.showLoadingUi();
    }

    void hideLoadingUi() {
        mLoadingView.hideLoadingUi();
    }

    void setLogoTopMargin(int topMargin) {
        mLogoView.setLogoTopMargin(topMargin);
    }

    void setLogoBottomMargin(int bottomMargin) {
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) getLayoutParams();
        marginLayoutParams.bottomMargin = bottomMargin;
        setLayoutParams(marginLayoutParams);
    }

    void setLogoHeight(int height) {
        mLogoView.setLogoHeight(height);
    }

    void endFadeAnimation() {
        mLogoView.endFadeAnimation();
    }

    void setAnimationEnabled(boolean enabled) {
        mLogoView.setAnimationEnabled(enabled);
    }

    void setClickHandler(LogoProperties.ClickHandler handler) {
        mLogoView.setClickHandler(handler);
    }

    void setLogoAvailableCallback(Callback<Logo> callback) {
        mLogoView.setLogoAvailableCallback(callback);
    }

    void setDoodleSize(int doodleSize) {
        mLogoView.setDoodleSize(doodleSize);
    }

    void destroy() {
        mLogoView.destroy();
        mLoadingView.destroy();
    }

    android.animation.@Nullable ObjectAnimator getFadeAnimationForTesting() {
        return mLogoView.getFadeAnimationForTesting(); // IN-TEST
    }

    android.graphics.@Nullable Bitmap getNewLogoDrawableBitmapForTesting() {
        return mLogoView.getNewLogoDrawableBitmapForTesting(); // IN-TEST
    }

    @Nullable Drawable getDefaultGoogleLogoDrawableForTesting() {
        return mLogoView.getDefaultGoogleLogoDrawableForTesting(); // IN-TEST
    }

    boolean getAnimationEnabledForTesting() {
        return mLogoView.getAnimationEnabledForTesting(); // IN-TEST
    }

    LogoProperties.@Nullable ClickHandler getClickHandlerForTesting() {
        return mLogoView.getClickHandlerForTesting(); // IN-TEST
    }

    int getDoodleSizeForTesting() {
        return mLogoView.getDoodleSizeForTesting(); // IN-TEST
    }

    void setLoadingViewVisibilityForTesting(int visibility) {
        mLoadingView.setVisibility(visibility);
    }

    int getLoadingViewVisibilityForTesting() {
        return mLoadingView.getVisibility();
    }
}
