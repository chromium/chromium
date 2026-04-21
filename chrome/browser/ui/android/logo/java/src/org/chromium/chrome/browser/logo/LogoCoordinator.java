// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.chrome.browser.logo.LogoUtils.getGoogleLogoDrawable;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

import java.util.Objects;
import java.util.function.Supplier;

/** Coordinator used to fetch and load logo image for Start surface and NTP. */
@NullMarked
public class LogoCoordinator {
    private final Context mContext;
    private final PropertyModel mLogoModel;
    // This supplier is only used when the NTP surface is in tablet mode.
    private final Supplier<Boolean> mIsInMultiWindowModeSupplier;
    private LogoMediator mMediator;
    private FrameLayout mLogoView;
    private boolean mIsInMultiWindowModeOnTablet;
    private NtpCustomizationConfigManager.@Nullable HomepageStateListener mHomepageStateListener;
    // The current tint color of logo if the DSE is Google. It is null when the default colorful
    // Google logo is used.
    private @Nullable Integer mLogoColor;

    /** Interface for the observers of the logo visibility change. */
    public interface VisibilityObserver {
        void onLogoVisibilityChanged();
    }

    /**
     * Creates a LogoCoordinator object.
     *
     * @param context Used to load colors and resources.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param parentView The parent view.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param visibilityObserver Observer object monitoring logo visibility.
     * @param isInMultiWindowModeSupplier The Supplier of whether the device is in multiple window
     *     mode.
     */
    public LogoCoordinator(
            Context context,
            Callback<LoadUrlParams> logoClickedCallback,
            ViewGroup parentView,
            Callback<Logo> onLogoAvailableCallback,
            @Nullable VisibilityObserver visibilityObserver,
            Supplier<Boolean> isInMultiWindowModeSupplier) {
        // TODO(crbug.com/40881870): This is weird that we're passing in our view,
        //  and we have to expose our view via getView. We shouldn't only have to do one of these.
        mContext = context;
        mLogoModel = new PropertyModel(LogoProperties.ALL_KEYS);
        mIsInMultiWindowModeSupplier = isInMultiWindowModeSupplier;

        ViewStub stub = parentView.findViewById(R.id.logo_view_stub);
        if (!ChromeFeatureList.sLogoViewRefactor.isEnabled()) {
            stub.setLayoutResource(R.layout.legacy_logo_view_layout);
        }
        stub.inflate();
        mLogoView = parentView.findViewById(R.id.search_provider_logo);

        if (ChromeFeatureList.sLogoViewRefactor.isEnabled()) {
            PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LogoViewBinder());
        } else {
            PropertyModelChangeProcessor.create(mLogoModel, mLogoView, new LegacyLogoViewBinder());
        }

        Drawable defaultGoogleLogoDrawable = getGoogleLogoDrawable(context);
        mLogoColor =
                NtpCustomizationUtils.setTintForDefaultGoogleLogo(
                        context, defaultGoogleLogoDrawable);
        mIsInMultiWindowModeOnTablet = mIsInMultiWindowModeSupplier.get();
        setDoodleSize(
                mIsInMultiWindowModeOnTablet
                        ? LogoUtils.DoodleSize.TABLET_SPLIT_SCREEN
                        : LogoUtils.DoodleSize.REGULAR);

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
                                context, newType, /* primaryColor= */ Color.WHITE);
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
     * @see LogoMediator#destroy
     */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mMediator.destroy();
        if (mLogoView instanceof LogoView) {
            ((LogoView) mLogoView).destroy();
        } else if (mLogoView instanceof LegacyLogoView) {
            ((LegacyLogoView) mLogoView).destroy();
        }
        mLogoView = null;
        if (mHomepageStateListener != null) {
            NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
            mHomepageStateListener = null;
        }
    }

    /**
     * Sets the width of the logo view in LayoutParams and clears its margins. This should be called
     * before the parent view's measure pass to avoid double measurement.
     *
     * @param widthPx The expected width of the logo view.
     */
    public void setLayoutWidth(int widthPx) {
        MarginLayoutParams layoutParams = (MarginLayoutParams) mLogoView.getLayoutParams();
        if (layoutParams.width == widthPx) {
            return;
        }

        layoutParams.width = widthPx;
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
        if (!mMediator.isDefaultGoogleLogoShown() || Objects.equals(mLogoColor, primaryColor)) {
            return;
        }

        mLogoColor = primaryColor;
        Drawable defaultGoogleLogoDrawable =
                ContextCompat.getDrawable(context, R.drawable.ic_google_logo);
        Drawable tintedDrawable =
                NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                        context, defaultGoogleLogoDrawable, backgroundType, primaryColor);
        mMediator.updateDefaultGoogleLogo(tintedDrawable);
    }

    /**
     * Adjusts the doodle size while the tablet transitions to or from a multi-screen layout,
     * ensuring the change occurs post-logo initialization.
     */
    public void updateDoodleOnTablet(boolean showingNonStandardGoogleLogo) {
        boolean isInMultiWindowModeOnTabletPreviousValue = mIsInMultiWindowModeOnTablet;
        mIsInMultiWindowModeOnTablet = mIsInMultiWindowModeSupplier.get();

        if (isInMultiWindowModeOnTabletPreviousValue == mIsInMultiWindowModeOnTablet) return;

        int doodleSize =
                mIsInMultiWindowModeOnTablet
                        ? LogoUtils.DoodleSize.TABLET_SPLIT_SCREEN
                        : LogoUtils.DoodleSize.REGULAR;
        setDoodleSize(doodleSize);

        if (showingNonStandardGoogleLogo) {
            int[] logoParams =
                    LogoUtils.getLogoViewLayoutParams(
                            mContext.getResources(), /* isLogoDoodle= */ true, doodleSize);
            mLogoModel.set(LogoProperties.LOGO_HEIGHT, logoParams[0]);
            mLogoModel.set(LogoProperties.LOGO_TOP_MARGIN, logoParams[1]);
        }
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
