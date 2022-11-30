// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * An implementation of {@link LogoView.Delegate}.
 */
public class LogoDelegateImpl implements LogoView.Delegate {
    // UMA enum constants. CTA means the "click-to-action" icon.
    private static final String LOGO_SHOWN_UMA_NAME = "NewTabPage.LogoShown";
    private static final String LOGO_SHOWN_FROM_CACHE_UMA_NAME = "NewTabPage.LogoShown.FromCache";
    private static final String LOGO_SHOWN_FRESH_UMA_NAME = "NewTabPage.LogoShown.Fresh";
    private static final int STATIC_LOGO_SHOWN = 0;
    private static final int CTA_IMAGE_SHOWN = 1;
    private static final int LOGO_SHOWN_COUNT = 2;

    private static final String LOGO_SHOWN_TIME_UMA_NAME = "NewTabPage.LogoShownTime2";

    private static final String LOGO_CLICK_UMA_NAME = "NewTabPage.LogoClick";
    private static final int STATIC_LOGO_CLICKED = 0;
    private static final int CTA_IMAGE_CLICKED = 1;
    private static final int ANIMATED_LOGO_CLICKED = 2;

    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final LogoView mLogoView;
    private final LogoBridge mLogoBridge;

    private ImageFetcher mImageFetcher;
    private String mOnLogoClickUrl;
    private String mAnimatedLogoUrl;

    private boolean mShouldRecordLoadTime = true;
    private boolean mIsDestroyed;

    /**
     * Construct a new {@link LogoDelegateImpl}.
     * @param logoClickedCallback A callback for loading the URL when the logo is clicked. May be
     *         null when click events are not supported.
     * @param logoView The view that shows the search provider logo. Maybe null when the client is
     *         controlling the View presentation itself.
     * @param profile The profile to show the logo for.
     */
    public LogoDelegateImpl(Callback<LoadUrlParams> logoClickedCallback,
            @Nullable LogoView logoView, Profile profile) {
        mLogoView = logoView;
        mLogoBridge = new LogoBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(
                ImageFetcherConfig.DISK_CACHE_ONLY, profile.getProfileKey());
        mLogoClickedCallback = logoClickedCallback;
    }

    public void destroy() {
        mIsDestroyed = true;
        mLogoBridge.destroy();
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    @Override
    public void onLogoClicked(boolean isAnimatedLogoShowing) {
        if (mIsDestroyed) return;

        if (!isAnimatedLogoShowing && mAnimatedLogoUrl != null) {
            RecordHistogram.recordSparseHistogram(LOGO_CLICK_UMA_NAME, CTA_IMAGE_CLICKED);
            mLogoView.showLoadingView();
            mImageFetcher.fetchGif(ImageFetcher.Params.create(mAnimatedLogoUrl,
                                           ImageFetcher.NTP_ANIMATED_LOGO_UMA_CLIENT_NAME),
                    (BaseGifImage animatedLogoImage) -> {
                        if (mIsDestroyed || animatedLogoImage == null) return;
                        mLogoView.playAnimatedLogo(animatedLogoImage);
                    });
        } else if (mOnLogoClickUrl != null) {
            RecordHistogram.recordSparseHistogram(LOGO_CLICK_UMA_NAME,
                    isAnimatedLogoShowing ? ANIMATED_LOGO_CLICKED : STATIC_LOGO_CLICKED);
            mLogoClickedCallback.onResult(new LoadUrlParams(mOnLogoClickUrl, PageTransition.LINK));
        }
    }

    public void getSearchProviderLogo(final LogoObserver logoObserver) {
        assert !mIsDestroyed;

        final long loadTimeStart = System.currentTimeMillis();

        LogoObserver wrapperCallback = new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (mIsDestroyed) return;

                if (logo != null) {
                    int logoType =
                            logo.animatedLogoUrl == null ? STATIC_LOGO_SHOWN : CTA_IMAGE_SHOWN;
                    RecordHistogram.recordEnumeratedHistogram(
                            LOGO_SHOWN_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    if (fromCache) {
                        RecordHistogram.recordEnumeratedHistogram(
                                LOGO_SHOWN_FROM_CACHE_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    } else {
                        RecordHistogram.recordEnumeratedHistogram(
                                LOGO_SHOWN_FRESH_UMA_NAME, logoType, LOGO_SHOWN_COUNT);
                    }
                    if (mShouldRecordLoadTime) {
                        long loadTime = System.currentTimeMillis() - loadTimeStart;
                        RecordHistogram.recordMediumTimesHistogram(
                                LOGO_SHOWN_TIME_UMA_NAME, loadTime);
                        // Only record the load time once per NTP, for the first logo we got,
                        // whether that came from cache or not.
                        mShouldRecordLoadTime = false;
                    }
                } else if (!fromCache) {
                    // If we got a fresh (i.e. not from cache) null logo, don't record any load
                    // time even if we get another update later.
                    mShouldRecordLoadTime = false;
                }

                mOnLogoClickUrl = logo != null ? logo.onClickUrl : null;
                mAnimatedLogoUrl =
                        (logo != null && mLogoView != null) ? logo.animatedLogoUrl : null;

                logoObserver.onLogoAvailable(logo, fromCache);
            }

            @Override
            public void onCachedLogoRevalidated() {
                logoObserver.onCachedLogoRevalidated();
            }
        };

        mLogoBridge.getCurrentLogo(wrapperCallback);
    }
}
