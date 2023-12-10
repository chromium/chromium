// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Mediator used to fetch and load logo image for Start surface and NTP.*/
public class LogoMediator implements TemplateUrlServiceObserver {
    // UMA enum constants. CTA means the "click-to-action" icon.
    private static final String LOGO_SHOWN_UMA_NAME = "NewTabPage.LogoShown";
    private static final String LOGO_SHOWN_FROM_CACHE_UMA_NAME = "NewTabPage.LogoShown.FromCache";
    private static final String LOGO_SHOWN_FRESH_UMA_NAME = "NewTabPage.LogoShown.Fresh";

    @IntDef({
        LogoShownId.STATIC_LOGO_SHOWN,
        LogoShownId.CTA_IMAGE_SHOWN,
        LogoShownId.LOGO_SHOWN_COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface LogoShownId {
        int STATIC_LOGO_SHOWN = 0;
        int CTA_IMAGE_SHOWN = 1;
        int LOGO_SHOWN_COUNT = 2;
    }

    private static final String LOGO_SHOWN_TIME_UMA_NAME = "NewTabPage.LogoShownTime2";

    private static final String LOGO_CLICK_UMA_NAME = "NewTabPage.LogoClick";

    @IntDef({
        LogoClickId.STATIC_LOGO_CLICKED,
        LogoClickId.CTA_IMAGE_CLICKED,
        LogoClickId.ANIMATED_LOGO_CLICKED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface LogoClickId {
        int STATIC_LOGO_CLICKED = 0;
        int CTA_IMAGE_CLICKED = 1;
        int ANIMATED_LOGO_CLICKED = 2;
    }

    private final PropertyModel mLogoModel;
    private final Context mContext;
    private Profile mProfile;
    private LogoBridge mLogoBridge;
    private ImageFetcher mImageFetcher;
    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final Callback<LogoBridge.Logo> mOnLogoAvailableRunnable;
    private final Runnable mOnCachedLogoRevalidatedRunnable;
    private boolean mHasLogoLoadedForCurrentSearchEngine;
    private final boolean mShouldFetchDoodle;
    private boolean mIsParentSurfaceShown; // This value should always be true when this class
    // is used by NTP.
    private final LogoCoordinator.VisibilityObserver mVisibilityObserver;
    private final CachedTintedBitmap mDefaultGoogleLogo;
    private boolean mShouldShowLogo;
    private boolean mIsLoadPending;
    private String mOnLogoClickUrl;
    private String mAnimatedLogoUrl;
    private boolean mShouldRecordLoadTime = true;

    private final ObserverList<LogoCoordinator.VisibilityObserver> mVisibilityObservers =
            new ObserverList<>();

    /**
     * Creates a LogoMediator object.
     *
     * @param context Used to load colors and resources.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoModel The model that is required to build the logo on start surface or ntp.
     * @param shouldFetchDoodle Whether to fetch doodle if there is.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param onCachedLogoRevalidatedRunnable The runnable for when cached logo is revalidated.
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             is true when this class is used by NTP; while used by Start,
     *                             it's only true on Start homepage.
     * @param visibilityObserver Observer object monitoring logo visibility.
     * @param defaultGoogleLogo The google logo shared across all NTPs when Google is the default
     *                          search engine.
     */
    LogoMediator(
            Context context,
            Callback<LoadUrlParams> logoClickedCallback,
            PropertyModel logoModel,
            boolean shouldFetchDoodle,
            Callback<LogoBridge.Logo> onLogoAvailableCallback,
            Runnable onCachedLogoRevalidatedRunnable,
            boolean isParentSurfaceShown,
            LogoCoordinator.VisibilityObserver visibilityObserver,
            CachedTintedBitmap defaultGoogleLogo) {
        mContext = context;
        mLogoModel = logoModel;
        mLogoClickedCallback = logoClickedCallback;
        mShouldFetchDoodle = shouldFetchDoodle;
        mOnLogoAvailableRunnable = onLogoAvailableCallback;
        mOnCachedLogoRevalidatedRunnable = onCachedLogoRevalidatedRunnable;
        mIsParentSurfaceShown = isParentSurfaceShown;
        mVisibilityObserver = visibilityObserver;
        mVisibilityObservers.addObserver(mVisibilityObserver);
        mDefaultGoogleLogo = defaultGoogleLogo;
    }

    /**
     * Initialize the mediator with the components that had native initialization dependencies,
     * i.e. Profile..
     */
    void initWithNative() {
        if (mProfile != null) return;

        mProfile = Profile.getLastUsedRegularProfile();
        updateVisibility();

        if (mShouldShowLogo) {
            showSearchProviderInitialView();
            if (mIsLoadPending) loadSearchProviderLogo(/* animationEnabled= */ false);
        }

        TemplateUrlServiceFactory.getForProfile(mProfile).addObserver(this);
    }

    /** Update the logo based on default search engine changes.*/
    @Override
    public void onTemplateURLServiceChanged() {
        mHasLogoLoadedForCurrentSearchEngine = false;
        loadSearchProviderLogoWithAnimation();
    }

    /** Force to load the search provider logo with animation enabled.*/
    void loadSearchProviderLogoWithAnimation() {
        updateVisibilityAndMaybeCleanUp(
                mIsParentSurfaceShown, /* shouldDestroyBridge= */ false, true);
    }

    /**
     * If it's on Start surface homepage or on NTP, load search provider logo; If it's not on Start
     * surface homepage, destroy the part of LogoBridge which includes mImageFetcher.
     *
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             should always be true when this class is used by NTP.
     * @param shouldDestroyBridge Whether to destroy the part of LogoBridge for saving memory. This
     *                              value should always be false when this class is used by NTP.
     *                              TODO(crbug.com/1315676): Remove this variable once the refactor
     *                              is launched and StartSurfaceState is removed. Now we check this
     *                              because there are some intermediate StartSurfaceStates,
     *                              i.e. SHOWING_START.
     * @param animationEnabled Whether to enable the fade in animation.
     */
    void updateVisibilityAndMaybeCleanUp(
            boolean isParentSurfaceShown, boolean shouldDestroyBridge, boolean animationEnabled) {
        assert !isParentSurfaceShown || !shouldDestroyBridge;

        mIsParentSurfaceShown = isParentSurfaceShown;
        updateVisibility();

        if (mShouldShowLogo) {
            if (mProfile != null) {
                loadSearchProviderLogo(animationEnabled);
            } else {
                mIsLoadPending = true;
            }
        } else if (shouldDestroyBridge && mLogoBridge != null) {
            mHasLogoLoadedForCurrentSearchEngine = false;
            // Destroy the part of logoBridge when hiding Start surface homepage to save memory.
            cleanUp();
        }
    }

    /** Cleans up any code as necessary.*/
    void destroy() {
        cleanUp();

        if (mProfile != null) {
            TemplateUrlServiceFactory.getForProfile(mProfile).removeObserver(this);
        }

        if (mVisibilityObserver != null) {
            mVisibilityObservers.removeObserver(mVisibilityObserver);
        }
    }

    private void cleanUp() {
        if (mLogoBridge != null) {
            mLogoBridge.destroy();
            mLogoBridge = null;
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
    }

    /** Returns whether LogoView is visible.*/
    boolean isLogoVisible() {
        return mShouldShowLogo && mLogoModel.get(LogoProperties.VISIBILITY);
    }

    /**
     * Load the search provider logo on Start surface.
     *
     * @param animationEnabled Whether to enable the fade in animation.
     */
    private void loadSearchProviderLogo(boolean animationEnabled) {
        // If logo is already updated for the current search provider, or profile is null or off the
        // record, don't bother loading the logo image.
        if (mHasLogoLoadedForCurrentSearchEngine || mProfile == null || !mShouldShowLogo) return;

        mHasLogoLoadedForCurrentSearchEngine = true;
        mLogoModel.set(LogoProperties.ANIMATION_ENABLED, animationEnabled);
        showSearchProviderInitialView();

        // If default search engine is google and doodle is not supported, doesn't bother to fetch
        // logo image.
        if (TemplateUrlServiceFactory.getForProfile(mProfile).isDefaultSearchEngineGoogle()
                && !mShouldFetchDoodle) {
            return;
        }

        if (mLogoBridge == null) {
            mLogoBridge = new LogoBridge(mProfile);
            mImageFetcher =
                    ImageFetcherFactory.createImageFetcher(
                            ImageFetcherConfig.DISK_CACHE_ONLY, mProfile.getProfileKey());
        }

        getSearchProviderLogo(
                new LogoBridge.LogoObserver() {
                    @Override
                    public void onLogoAvailable(LogoBridge.Logo logo, boolean fromCache) {
                        if (logo == null) {
                            if (fromCache) {
                                // There is no cached logo. Wait until we know whether there's a
                                // fresh one before making any further decisions.
                                return;
                            }
                            mLogoModel.set(
                                    LogoProperties.DEFAULT_GOOGLE_LOGO,
                                    getDefaultGoogleLogo(mContext));
                        }
                        mLogoModel.set(
                                LogoProperties.LOGO_CLICK_HANDLER,
                                LogoMediator.this::onLogoClicked);
                        mLogoModel.set(LogoProperties.LOGO, logo);

                        if (mOnLogoAvailableRunnable != null) {
                            mOnLogoAvailableRunnable.onResult(logo);
                        }
                    }

                    @Override
                    public void onCachedLogoRevalidated() {
                        if (mOnCachedLogoRevalidatedRunnable != null) {
                            mOnCachedLogoRevalidatedRunnable.run();
                        }
                    }
                });
    }

    private void showSearchProviderInitialView() {
        mLogoModel.set(LogoProperties.DEFAULT_GOOGLE_LOGO, getDefaultGoogleLogo(mContext));
        mLogoModel.set(LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW, true);
    }

    private void updateVisibility() {
        boolean doesDseHaveLogo =
                mProfile != null
                        ? TemplateUrlServiceFactory.getForProfile(mProfile)
                                .doesDefaultSearchEngineHaveLogo()
                        : ChromeSharedPreferences.getInstance()
                                .readBoolean(APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
        mShouldShowLogo = mIsParentSurfaceShown && doesDseHaveLogo;
        mLogoModel.set(LogoProperties.VISIBILITY, mShouldShowLogo);
        for (LogoCoordinator.VisibilityObserver observer : mVisibilityObservers) {
            observer.onLogoVisibilityChanged();
        }
    }

    /**
     * Get the default Google logo if available.
     * @param context Used to load colors and resources.
     * @return The default Google logo.
     */
    @VisibleForTesting
    Bitmap getDefaultGoogleLogo(Context context) {
        return TemplateUrlServiceFactory.getForProfile(mProfile).isDefaultSearchEngineGoogle()
                ? mDefaultGoogleLogo.getBitmap(context)
                : null;
    }

    public void onLogoClicked(boolean isAnimatedLogoShowing) {
        if (mLogoBridge == null) return;

        if (!isAnimatedLogoShowing && mAnimatedLogoUrl != null) {
            RecordHistogram.recordSparseHistogram(
                    LOGO_CLICK_UMA_NAME, LogoClickId.CTA_IMAGE_CLICKED);
            mLogoModel.set(LogoProperties.SHOW_LOADING_VIEW, true);
            mImageFetcher.fetchGif(
                    ImageFetcher.Params.create(
                            mAnimatedLogoUrl, ImageFetcher.NTP_ANIMATED_LOGO_UMA_CLIENT_NAME),
                    (BaseGifImage animatedLogoImage) -> {
                        if (mLogoBridge == null || animatedLogoImage == null) return;
                        mLogoModel.set(LogoProperties.ANIMATED_LOGO, animatedLogoImage);
                    });
        } else if (mOnLogoClickUrl != null) {
            RecordHistogram.recordSparseHistogram(
                    LOGO_CLICK_UMA_NAME,
                    isAnimatedLogoShowing
                            ? LogoClickId.ANIMATED_LOGO_CLICKED
                            : LogoClickId.STATIC_LOGO_CLICKED);
            mLogoClickedCallback.onResult(new LoadUrlParams(mOnLogoClickUrl, PageTransition.LINK));
        }
    }

    private void getSearchProviderLogo(final LogoObserver logoObserver) {
        assert mLogoBridge != null;

        final long loadTimeStart = System.currentTimeMillis();

        LogoObserver wrapperCallback =
                new LogoObserver() {
                    @Override
                    public void onLogoAvailable(Logo logo, boolean fromCache) {
                        if (mLogoBridge == null) return;

                        if (logo != null) {
                            int logoType =
                                    logo.animatedLogoUrl == null
                                            ? LogoShownId.STATIC_LOGO_SHOWN
                                            : LogoShownId.CTA_IMAGE_SHOWN;
                            RecordHistogram.recordEnumeratedHistogram(
                                    LOGO_SHOWN_UMA_NAME, logoType, LogoShownId.LOGO_SHOWN_COUNT);
                            if (fromCache) {
                                RecordHistogram.recordEnumeratedHistogram(
                                        LOGO_SHOWN_FROM_CACHE_UMA_NAME,
                                        logoType,
                                        LogoShownId.LOGO_SHOWN_COUNT);
                            } else {
                                RecordHistogram.recordEnumeratedHistogram(
                                        LOGO_SHOWN_FRESH_UMA_NAME,
                                        logoType,
                                        LogoShownId.LOGO_SHOWN_COUNT);
                            }
                            if (mShouldRecordLoadTime) {
                                long loadTime = System.currentTimeMillis() - loadTimeStart;
                                RecordHistogram.recordMediumTimesHistogram(
                                        LOGO_SHOWN_TIME_UMA_NAME, loadTime);
                                // Only record the load time once per NTP, for the first logo we
                                // got, whether that came from cache or not.
                                mShouldRecordLoadTime = false;
                            }
                        } else if (!fromCache) {
                            // If we got a fresh (i.e. not from cache) null logo, don't record any
                            // load time even if we get another update later.
                            mShouldRecordLoadTime = false;
                        }

                        mOnLogoClickUrl = logo != null ? logo.onClickUrl : null;
                        mAnimatedLogoUrl = logo != null ? logo.animatedLogoUrl : null;

                        logoObserver.onLogoAvailable(logo, fromCache);
                    }

                    @Override
                    public void onCachedLogoRevalidated() {
                        logoObserver.onCachedLogoRevalidated();
                    }
                };

        mLogoBridge.getCurrentLogo(wrapperCallback);
    }

    // TODO(crbug.com/1394983): Remove the following ForTesting methods if possible.
    void setHasLogoLoadedForCurrentSearchEngineForTesting(
            boolean hasLogoLoadedForCurrentSearchEngine) {
        mHasLogoLoadedForCurrentSearchEngine = hasLogoLoadedForCurrentSearchEngine;
    }

    void setLogoBridgeForTesting(LogoBridge logoBridge) {
        mLogoBridge = logoBridge;
    }

    void setImageFetcherForTesting(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    void setAnimatedLogoUrlForTesting(String animatedLogoUrl) {
        mAnimatedLogoUrl = animatedLogoUrl;
    }

    void setOnLogoClickUrlForTesting(String onLogoClickUrl) {
        mOnLogoClickUrl = onLogoClickUrl;
    }

    ImageFetcher getImageFetcherForTesting() {
        return mImageFetcher;
    }

    LogoBridge getLogoBridgeForTesting() {
        return mLogoBridge;
    }

    boolean getIsLoadPendingForTesting() {
        return mIsLoadPending;
    }
}
