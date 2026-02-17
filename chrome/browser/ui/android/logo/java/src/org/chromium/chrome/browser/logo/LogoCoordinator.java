// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.chrome.browser.logo.LogoUtils.getGoogleLogoDrawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View.MeasureSpec;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator used to fetch and load logo image for Start surface and NTP. */
@NullMarked
public class LogoCoordinator {
    private final PropertyModel mLogoModel;
    private LogoMediator mMediator;
    private LogoView mLogoView;
    private NtpCustomizationConfigManager.@Nullable HomepageStateListener mHomepageStateListener;

    /** Interface for the observers of the logo visibility change. */
    public interface VisibilityObserver {
        void onLogoVisibilityChanged();
    }

    /**
     * Creates a LogoCoordinator object.
     *
     * @param context Used to load colors and resources.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoView The view that shows the search provider logo.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param visibilityObserver Observer object monitoring logo visibility.
     */
    public LogoCoordinator(
            Context context,
            Callback<LoadUrlParams> logoClickedCallback,
            LogoView logoView,
            Callback<Logo> onLogoAvailableCallback,
            @Nullable VisibilityObserver visibilityObserver) {
        // TODO(crbug.com/40881870): This is weird that we're passing in our view,
        //  and we have to expose our view via getView. We shouldn't only have to do one of these.
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mLogoView = logoView;
        PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());

        Drawable defaultGoogleLogoDrawable = getGoogleLogoDrawable(context);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(context, defaultGoogleLogoDrawable);

        mMediator =
                new LogoMediator(
                        logoClickedCallback,
                        mLogoModel,
                        onLogoAvailableCallback,
                        visibilityObserver,
                        defaultGoogleLogoDrawable);

        // Should be called after mMediator is created.
        maybeInitHomepageStateListener(context);
    }

    private void maybeInitHomepageStateListener(Context context) {
        if (!NtpCustomizationUtils.isNtpThemeCustomizationEnabled()) {
            return;
        }

        mHomepageStateListener =
                new NtpCustomizationConfigManager.HomepageStateListener() {
                    @Override
                    public void onBackgroundImageChanged(
                            Bitmap originalBitmap,
                            BackgroundImageInfo backgroundImageInfo,
                            boolean fromInitialization,
                            int oldType,
                            int newType) {
                        maybeUpdateTintForDefaultGoogleLogo(
                                context, newType, /* primaryColor= */ null);
                    }

                    @Override
                    public void onBackgroundColorChanged(
                            @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                            int backgroundColor,
                            boolean fromInitialization,
                            int oldType,
                            int newType) {
                        @ColorInt
                        Integer primaryColor =
                                ntpThemeColorInfo == null
                                        ? null
                                        : NtpThemeColorUtils.getPrimaryColorFromColorInfo(
                                                context, ntpThemeColorInfo);
                        maybeUpdateTintForDefaultGoogleLogo(context, newType, primaryColor);
                    }

                    @Override
                    public void onBackgroundReset(@NtpBackgroundType int oldType) {
                        if (oldType == NtpBackgroundType.DEFAULT) return;

                        maybeUpdateTintForDefaultGoogleLogo(
                                context, NtpBackgroundType.DEFAULT, /* primaryColor= */ null);
                    }
                };
        // Skips being notified from NtpCustomizationConfigManager since the drawable has been
        // tinted if necessary when the initial logo view is shown.
        NtpCustomizationConfigManager.getInstance()
                .addListener(mHomepageStateListener, context, /* skipNotify= */ true);
    }

    /**
     * @see LogoMediator#initWithNative(Profile)
     */
    public void initWithNative(Profile profile) {
        // TODO(crbug.com/40881870): Would be more elegant if we were given an
        //  onNativeInitializedObserver and didn't rely on the good will of outside callers to
        //  invoke this.
        mMediator.initWithNative(profile);
    }

    /**
     * @see LogoMediator#loadSearchProviderLogoWithAnimation
     */
    public void loadSearchProviderLogoWithAnimation() {
        mMediator.loadSearchProviderLogoWithAnimation();
    }

    /**
     * @see LogoMediator#updateVisibility
     */
    public void updateVisibility(boolean animationEnabled) {
        mMediator.updateVisibility(animationEnabled);
    }

    /**
     * @see LogoMediator#destroy
     */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mMediator.destroy();
        mLogoView.destroy();
        mLogoView = null;
        if (mHomepageStateListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }
    }

    /**
     * Convenience method to call measure() on the logo view with MeasureSpecs converted from the
     * given dimensions (in pixels) with MeasureSpec.EXACTLY.
     */
    public void measureExactlyLogoView(int widthPx) {
        mLogoView.measure(
                MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mLogoView.getMeasuredHeight(), MeasureSpec.EXACTLY));
    }

    /** Jumps to the end of the logo view's cross-fading animation, if any.*/
    public void endFadeAnimation() {
        mLogoModel.set(LogoProperties.SET_END_FADE_ANIMATION, true);
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setAlpha(float alpha) {
        mLogoModel.set(LogoProperties.ALPHA, alpha);
    }

    /**
     * Sets the top margin of the logo view.
     *
     * @param topMargin The expected top margin.
     */
    public void setTopMargin(int topMargin) {
        mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, topMargin);
    }

    /**
     * Sets the bottom margin of the logo view.
     *
     * @param bottomMargin The expected bottom margin.
     */
    public void setBottomMargin(int bottomMargin) {
        mLogoModel.set(LogoProperties.LOGO_BOTTOM_MARGIN, bottomMargin);
    }

    /**
     * Updates the logo size to use when logo is a google doodle.
     *
     * @param doodleSize The logo size to use when logo is a google doodle.
     */
    public void setDoodleSize(int doodleSize) {
        mLogoModel.set(LogoProperties.DOODLE_SIZE, doodleSize);
    }

    /**
     * Updates the default Google logo with a tint color if it is shown.
     *
     * @param context The context to get themed color.
     * @param backgroundType The NTP's background theme type.
     * @param primaryColor The primary color is selected.
     */
    private void maybeUpdateTintForDefaultGoogleLogo(
            Context context,
            @NtpBackgroundType int backgroundType,
            @Nullable @ColorInt Integer primaryColor) {
        // If the default Google logo isn't shown, returns here.
        if (!mMediator.isDefaultGoogleLogoShown()) return;

        Drawable defaultGoogleLogoDrawable =
                ContextCompat.getDrawable(context, R.drawable.ic_google_logo);
        Drawable tintedDrawable =
                NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                        context, defaultGoogleLogoDrawable, backgroundType, primaryColor);
        mMediator.updateDefaultGoogleLogo(tintedDrawable);
    }

    /**
     * @see LogoMediator#onTemplateURLServiceChanged
     */
    public void onTemplateURLServiceChangedForTesting() {
        mMediator.resetSearchEngineKeywordForTesting(); // IN-TEST
        mMediator.onTemplateURLServiceChanged();
    }

    /** @see LogoMediator#onLogoClicked */
    public void onLogoClickedForTesting(boolean isAnimatedLogoShowing) {
        mMediator.onLogoClicked(isAnimatedLogoShowing);
    }

    public void setLogoBridgeForTesting(LogoBridge logoBridge) {
        mMediator.setLogoBridgeForTesting(logoBridge);
    }

    public void setOnLogoClickUrlForTesting(String onLogoClickUrl) {
        mMediator.setOnLogoClickUrlForTesting(onLogoClickUrl);
    }

    void setMediatorForTesting(LogoMediator mediator) {
        mMediator = mediator;
    }
}
