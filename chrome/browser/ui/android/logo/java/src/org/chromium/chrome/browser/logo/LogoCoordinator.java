// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Coordinator used to fetch and load logo image for Start surface and NTP.
 */
public class LogoCoordinator implements TemplateUrlServiceObserver {
    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final LogoView mLogoView;
    private final Callback<Logo> mOnLogoAvailableRunnable;
    private final Runnable mOnCachedLogoRevalidatedRunnable;

    private LogoDelegateImpl mLogoDelegate;
    private Profile mProfile;
    private boolean mHasLogoLoadedForCurrentSearchEngine;
    private boolean mShouldFetchDoodle;
    private boolean mIsParentSurfaceShown; // This value should always be true when this class
                                           // is used by NTP.
    private boolean mShouldShowLogo;
    private boolean mIsNativeInitialized;
    private boolean mIsLoadPending;

    /**
     * Creates a LogoCoordinator object.
     *
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoView The view that shows the search provider logo.
     * @param shouldFetchDoodle Whether to fetch doodle if there is.
     * @param onLogoAvailableCallback The callback for when logo is available.
     * @param onCachedLogoRevalidatedRunnable The runnable for when cached logo is revalidated.
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             is true when this class is used by NTP; while used by Start,
     *                             it's only true on Start homepage.
     */
    public LogoCoordinator(Callback<LoadUrlParams> logoClickedCallback, LogoView logoView,
            boolean shouldFetchDoodle, Callback<Logo> onLogoAvailableCallback,
            Runnable onCachedLogoRevalidatedRunnable, boolean isParentSurfaceShown) {
        mLogoClickedCallback = logoClickedCallback;
        mLogoView = logoView;
        mShouldFetchDoodle = shouldFetchDoodle;
        mOnLogoAvailableRunnable = onLogoAvailableCallback;
        mOnCachedLogoRevalidatedRunnable = onCachedLogoRevalidatedRunnable;
        mIsParentSurfaceShown = isParentSurfaceShown;
    }

    /**
     * Initialize the coordinator with the components that had native initialization dependencies,
     * i.e. Profile..
     */
    public void initWithNative() {
        if (mIsNativeInitialized) return;

        mIsNativeInitialized = true;
        mProfile = Profile.getLastUsedRegularProfile();
        updateVisibility();

        if (mShouldShowLogo) {
            showSearchProviderInitialView();
            if (mIsLoadPending) loadSearchProviderLogo(/*animationEnabled=*/false);
        }

        TemplateUrlServiceFactory.get().addObserver(this);
    }

    /**
     * Update the logo based on default search engine changes.
     */
    @Override
    public void onTemplateURLServiceChanged() {
        loadSearchProviderLogoWithAnimation();
    }

    /**
     * Force to load the search provider logo with animation enabled.
     */
    public void loadSearchProviderLogoWithAnimation() {
        mHasLogoLoadedForCurrentSearchEngine = false;
        maybeLoadSearchProviderLogo(mIsParentSurfaceShown, /*shouldDestroyDelegate=*/false, true);
    }

    /**
     * If it's on Start surface homepage or on NTP, load search provider logo; If it's not on Start
     * surface homepage, destroy mLogoDelegate.
     *
     * @param isParentSurfaceShown Whether Start surface homepage or NTP is shown. This value
     *                             should always be true when this class is used by NTP.
     * @param shouldDestroyDelegate Whether to destroy delegate for saving memory. This value should
     *                              always be false when this class is used by NTP.
     *                              TODO(crbug.com/1315676): Remove this variable once the refactor
     *                              is launched and StartSurfaceState is removed. Now we check this
     *                              because there are some intermediate StartSurfaceStates,
     *                              i.e. SHOWING_START.
     * @param animationEnabled Whether to enable the fade in animation.
     */
    public void maybeLoadSearchProviderLogo(
            boolean isParentSurfaceShown, boolean shouldDestroyDelegate, boolean animationEnabled) {
        assert !isParentSurfaceShown || !shouldDestroyDelegate;

        mIsParentSurfaceShown = isParentSurfaceShown;
        updateVisibility();

        if (mShouldShowLogo) {
            if (mProfile != null) {
                loadSearchProviderLogo(animationEnabled);
            } else {
                mIsLoadPending = true;
            }
        } else if (shouldDestroyDelegate && mLogoDelegate != null) {
            mHasLogoLoadedForCurrentSearchEngine = false;
            // Destroy |mLogoDelegate| when hiding Start surface homepage to save memory.
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
    }

    /**
     * Cleans up any code as necessary.
     */
    public void destroy() {
        if (mLogoDelegate != null) {
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }

        if (mLogoView != null) {
            mLogoView.destroy();
        }

        if (mIsNativeInitialized) {
            TemplateUrlServiceFactory.get().removeObserver(this);
        }
    }

    /**
     * Returns the LogoView.
     */
    public LogoView getView() {
        return mLogoView;
    }

    /**
     * Jumps to the end of the logo view's cross-fading animation, if any.
     */
    public void endFadeAnimation() {
        mLogoView.endFadeAnimation();
    }

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    public void setAlpha(float alpha) {
        mLogoView.setAlpha(alpha);
    }

    /**
     * Sets the top margin of the logo view.
     *
     * TODO(crbug.com/1359422): Move this inside View class and use ViewBinder as the bridge.
     * @param topMargin The expected top margin.
     */
    public void setTopMargin(int topMargin) {
        ((MarginLayoutParams) mLogoView.getLayoutParams()).topMargin = topMargin;
    }

    /**
     * Sets the bottom margin of the logo view.
     *
     * TODO(crbug.com/1359422): Move this inside View class and use ViewBinder as the bridge.
     * @param bottomMargin The expected bottom margin.
     */
    public void setBottomMargin(int bottomMargin) {
        ((MarginLayoutParams) mLogoView.getLayoutParams()).bottomMargin = bottomMargin;
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
        mLogoView.setAnimationEnabled(animationEnabled);
        mLogoView.showSearchProviderInitialView();

        // If default search engine is google and doodle is not supported, doesn't bother to fetch
        // logo image.
        if (TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle() && !mShouldFetchDoodle) {
            return;
        }

        if (mLogoDelegate == null) {
            mLogoDelegate = new LogoDelegateImpl(mLogoClickedCallback, mLogoView, mProfile);
        }

        mLogoDelegate.getSearchProviderLogo(new LogoObserver() {
            @Override
            public void onLogoAvailable(Logo logo, boolean fromCache) {
                if (logo == null && fromCache) {
                    // There is no cached logo. Wait until we know whether there's a fresh
                    // one before making any further decisions.
                    return;
                }
                mLogoView.setDelegate(mLogoDelegate);
                mLogoView.updateLogo(logo);

                if (mOnLogoAvailableRunnable != null) mOnLogoAvailableRunnable.onResult(logo);
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
        mLogoView.showSearchProviderInitialView();
    }

    private void updateVisibility() {
        mShouldShowLogo = mIsParentSurfaceShown
                && (!mIsNativeInitialized
                        || TemplateUrlServiceFactory.get().doesDefaultSearchEngineHaveLogo());

        mLogoView.setVisibility(mShouldShowLogo ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    public boolean isLogoVisibleForTesting() {
        return mShouldShowLogo;
    }

    @VisibleForTesting
    void setShouldFetchDoodleForTesting(boolean shouldFetchDoodle) {
        mShouldFetchDoodle = shouldFetchDoodle;
    }

    @VisibleForTesting
    void setLogoDelegateForTesting(LogoDelegateImpl logoDelegate) {
        mLogoDelegate = logoDelegate;
    }

    @VisibleForTesting
    void setHasLogoLoadedForCurrentSearchEngineForTesting(
            boolean hasLogoLoadedForCurrentSearchEngine) {
        mHasLogoLoadedForCurrentSearchEngine = hasLogoLoadedForCurrentSearchEngine;
    }

    @VisibleForTesting
    boolean getIsLoadPendingForTesting() {
        return mIsLoadPending;
    }

    @VisibleForTesting
    LogoDelegateImpl getLogoDelegateForTesting() {
        return mLogoDelegate;
    }
}
