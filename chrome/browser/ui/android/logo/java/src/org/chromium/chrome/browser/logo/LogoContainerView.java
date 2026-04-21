// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;

/** Container view for the logo and loading view. */
@NullMarked
public class LogoContainerView extends FrameLayout {
    private @Nullable LogoView mLogoView;

    public LogoContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public void updateLogo(Logo logo) {
        assumeNonNull(mLogoView).updateLogo(logo);
    }

    public void endAnimationsForTesting() {
        assumeNonNull(mLogoView).endAnimationsForTesting();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLogoView = findViewById(R.id.search_provider_logo);
    }

    void showSearchProviderInitialView() {
        assumeNonNull(mLogoView).showSearchProviderInitialView();
    }

    void playAnimatedLogo(Object animatedLogo) {
        assumeNonNull(mLogoView).playAnimatedLogo(animatedLogo);
    }

    void setDefaultGoogleLogoDrawable(Drawable defaultGoogleLogoDrawable) {
        assumeNonNull(mLogoView).setDefaultGoogleLogoDrawable(defaultGoogleLogoDrawable);
    }

    boolean maybeShowDefaultLogoDrawable() {
        return assumeNonNull(mLogoView).maybeShowDefaultLogoDrawable();
    }

    void showLoadingView() {
        assumeNonNull(mLogoView).showLoadingView();
    }

    void setLogoTopMargin(int topMargin) {
        assumeNonNull(mLogoView).setLogoTopMargin(topMargin);
    }

    void setLogoBottomMargin(int bottomMargin) {
        MarginLayoutParams marginLayoutParams = (MarginLayoutParams) getLayoutParams();
        marginLayoutParams.bottomMargin = bottomMargin;
        setLayoutParams(marginLayoutParams);
    }

    void setLogoHeight(int height) {
        assumeNonNull(mLogoView).setLogoHeight(height);
    }

    void endFadeAnimation() {
        assumeNonNull(mLogoView).endFadeAnimation();
    }

    void setAnimationEnabled(boolean enabled) {
        assumeNonNull(mLogoView).setAnimationEnabled(enabled);
    }

    void setClickHandler(LogoProperties.ClickHandler handler) {
        assumeNonNull(mLogoView).setClickHandler(handler);
    }

    void setLogoAvailableCallback(Callback<Logo> callback) {
        assumeNonNull(mLogoView).setLogoAvailableCallback(callback);
    }

    void setDoodleSize(int doodleSize) {
        assumeNonNull(mLogoView).setDoodleSize(doodleSize);
    }

    void destroy() {
        assumeNonNull(mLogoView).destroy();
        mLogoView = null;
    }

    android.animation.@Nullable ObjectAnimator getFadeAnimationForTesting() {
        return assumeNonNull(mLogoView).getFadeAnimationForTesting();
    }

    android.graphics.@Nullable Bitmap getNewLogoDrawableBitmapForTesting() {
        return assumeNonNull(mLogoView).getNewLogoDrawableBitmapForTesting();
    }

    @Nullable Drawable getDefaultGoogleLogoDrawableForTesting() {
        return assumeNonNull(mLogoView).getDefaultGoogleLogoDrawableForTesting();
    }

    boolean getAnimationEnabledForTesting() {
        return assumeNonNull(mLogoView).getAnimationEnabledForTesting();
    }

    LogoProperties.@Nullable ClickHandler getClickHandlerForTesting() {
        return assumeNonNull(mLogoView).getClickHandlerForTesting();
    }

    int getDoodleSizeForTesting() {
        return assumeNonNull(mLogoView).getDoodleSizeForTesting();
    }

    void setLoadingViewVisibilityForTesting(int visibility) {
        assumeNonNull(mLogoView).setLoadingViewVisibilityForTesting(visibility);
    }

    int getLoadingViewVisibilityForTesting() {
        return assumeNonNull(mLogoView).getLoadingViewVisibilityForTesting();
    }
}
