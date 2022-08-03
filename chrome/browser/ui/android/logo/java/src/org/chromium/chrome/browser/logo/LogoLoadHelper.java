// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.chrome.browser.logo.LogoBridge.LogoObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Helper used to fetch and load logo image for Start surface.
 */
public class LogoLoadHelper {
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<LoadUrlParams> mLogoClickedCallback;
    private final LogoView mLogoView;

    private CallbackController mCallbackController = new CallbackController();
    private LogoDelegateImpl mLogoDelegate;
    private boolean mHasLogoLoadedForCurrentSearchEngine;

    /**
     * Creates a LogoLoadHelper object.
     *
     * @param profileSupplier Supplies the currently applicable profile.
     * @param logoClickedCallback Supplies the StartSurface's parent tab.
     * @param logoView The view that shows the search provider logo.
     */
    public LogoLoadHelper(ObservableSupplier<Profile> profileSupplier,
            Callback<LoadUrlParams> logoClickedCallback, LogoView logoView) {
        mProfileSupplier = profileSupplier;
        mLogoClickedCallback = logoClickedCallback;
        mLogoView = logoView;
    }

    /**
     * If it's on Start surface homepage, load search provider logo; If it's not on start surface
     * homepage, destroy mLogoDelegate.
     *
     * @param isStartSurfaceShown Whether Start surface homepage is shown.
     * @param deprecatedStartSurfaceStateMarkedHidden Whether Start surface homepage is hidden.
     *         TODO(crbug.com/1315676): Remove this variable once the refactor is launched and
     *         StartSurfaceState is removed. Now we check this because there are some intermediate
     *         StartSurfaceStates, i.e. SHOWING_START.
     */
    public void maybeLoadSearchProviderLogoOnHomepage(
            boolean isStartSurfaceShown, boolean deprecatedStartSurfaceStateMarkedHidden) {
        if (isStartSurfaceShown) {
            if (mProfileSupplier.hasValue()) {
                loadSearchProviderLogo(/*animationEnabled=*/false);
                return;
            }
            assert mCallbackController != null;
            new OneShotCallback<>(mProfileSupplier, mCallbackController.makeCancelable(profile -> {
                assert profile != null : "Unexpectedly null profile from TabModel.";
                loadSearchProviderLogo(/*animationEnabled=*/false);
            }));
        } else if (deprecatedStartSurfaceStateMarkedHidden && mLogoDelegate != null) {
            mHasLogoLoadedForCurrentSearchEngine = false;
            // Destroy |mLogoDelegate| when hiding Start surface homepage to save memory.
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
    }

    /**
     * Update the logo based on default search engine changes.
     */
    public void onDefaultSearchEngineChanged() {
        mHasLogoLoadedForCurrentSearchEngine = false;
        loadSearchProviderLogo(/*animationEnabled=*/true);
    }

    /**
     * Cleans up any code as necessary.
     */
    public void destroy() {
        if (mLogoDelegate != null) {
            mLogoDelegate.destroy();
            mLogoDelegate = null;
        }
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    /**
     * Load the search provider logo on Start surface.
     *
     * @param animationEnabled Whether to enable the fade in animation.
     */
    private void loadSearchProviderLogo(boolean animationEnabled) {
        // If logo is already updated for the current search provider, or profile is null or off the
        // record, don't bother loading the logo image.
        if (mHasLogoLoadedForCurrentSearchEngine || !mProfileSupplier.hasValue()
                || mProfileSupplier.get().isOffTheRecord()
                || !TemplateUrlServiceFactory.get().doesDefaultSearchEngineHaveLogo()) {
            return;
        }

        if (mLogoDelegate == null) {
            mLogoDelegate =
                    new LogoDelegateImpl(mLogoClickedCallback, mLogoView, mProfileSupplier.get());
        }

        mHasLogoLoadedForCurrentSearchEngine = true;
        mLogoView.setAnimationEnabled(animationEnabled);
        mLogoView.showSearchProviderInitialView();

        // If default search engine is google and doodle is not supported, doesn't bother to fetch
        // logo image.
        if (TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()
                && !StartSurfaceConfiguration.IS_DOODLE_SUPPORTED.getValue()) {
            return;
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
            }

            @Override
            public void onCachedLogoRevalidated() {}
        });
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
}
