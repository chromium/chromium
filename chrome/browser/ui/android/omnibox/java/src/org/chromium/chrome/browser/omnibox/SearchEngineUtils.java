// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Common Default Search Engine functions. */
public class SearchEngineUtils implements Destroyable, TemplateUrlServiceObserver {
    private static final String TAG = "DSEUtils";
    private static ProfileKeyedMap<SearchEngineUtils> sProfileKeyedUtils =
            ProfileKeyedMap.createMapOfDestroyables();
    private static SearchEngineUtils sInstanceForTesting;

    private final @NonNull Profile mProfile;
    private final boolean mIsOffTheRecord;
    private final @NonNull TemplateUrlService mTemplateUrlService;
    private final @NonNull FaviconHelper mFaviconHelper;
    private final int mSearchEngineLogoTargetSizePixels;
    private Boolean mNeedToCheckForSearchEnginePromo;
    private boolean mDoesDefaultSearchEngineHaveLogo;
    private @Nullable StatusIconResource mSearchEngineLogo;

    /**
     * AndroidSearchEngineLogoEvents defined in tools/metrics/histograms/enums.xml. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @VisibleForTesting
    @IntDef({
        Events.FETCH_NON_GOOGLE_LOGO_REQUEST,
        Events.FETCH_FAILED_NULL_URL,
        Events.FETCH_FAILED_FAVICON_HELPER_ERROR,
        Events.FETCH_FAILED_RETURNED_BITMAP_NULL,
        Events.FETCH_SUCCESS_CACHE_HIT,
        Events.FETCH_SUCCESS
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface Events {
        int FETCH_NON_GOOGLE_LOGO_REQUEST = 0;
        int FETCH_FAILED_NULL_URL = 1;
        int FETCH_FAILED_FAVICON_HELPER_ERROR = 2;
        int FETCH_FAILED_RETURNED_BITMAP_NULL = 3;
        int FETCH_SUCCESS_CACHE_HIT = 4;
        int FETCH_SUCCESS = 5;

        int MAX = 6;
    }

    @VisibleForTesting
    SearchEngineUtils(Profile profile, FaviconHelper faviconHelper) {
        mProfile = profile;
        mIsOffTheRecord = profile.isOffTheRecord();
        mFaviconHelper = faviconHelper;

        mSearchEngineLogoTargetSizePixels =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_search_engine_logo_favicon_size);

        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this);
        onTemplateURLServiceChanged();
    }

    /** Get the instance of SearchEngineUtils associated with the supplied Profile. */
    public static SearchEngineUtils getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstanceForTesting != null) return sInstanceForTesting;

        assert profile != null;
        return sProfileKeyedUtils.getForProfile(profile, SearchEngineUtils::buildForProfile);
    }

    private static SearchEngineUtils buildForProfile(Profile profile) {
        return new SearchEngineUtils(profile, new FaviconHelper());
    }

    @Override
    public void destroy() {
        mTemplateUrlService.removeObserver(this);
    }

    @Override
    public void onTemplateURLServiceChanged() {
        mDoesDefaultSearchEngineHaveLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();

        if (mTemplateUrlService.isDefaultSearchEngineGoogle()) {
            mSearchEngineLogo = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
        } else {
            mSearchEngineLogo = null;
            recordEvent(Events.FETCH_NON_GOOGLE_LOGO_REQUEST);

            var templateUrl = mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
            if (templateUrl == null) {
                recordEvent(Events.FETCH_FAILED_NULL_URL);
                return;
            }

            var logoUrl = new GURL(templateUrl.getURL()).getOrigin();

            boolean willCall =
                    mFaviconHelper.getLocalFaviconImageForURL(
                            mProfile,
                            logoUrl,
                            mSearchEngineLogoTargetSizePixels,
                            (image, iconUrl) -> {
                                if (image == null) {
                                    recordEvent(Events.FETCH_FAILED_RETURNED_BITMAP_NULL);
                                    return;
                                }
                                mSearchEngineLogo =
                                        new StatusIconResource(logoUrl.getSpec(), image, 0);
                                recordEvent(Events.FETCH_SUCCESS);
                            });

            if (!willCall) recordEvent(Events.FETCH_FAILED_FAVICON_HELPER_ERROR);
        }
    }

    /** Returns whether the search engine logo should be shown. */
    public boolean shouldShowSearchEngineLogo() {
        return !mIsOffTheRecord;
    }

    /**
     * Get the search engine logo favicon. This can return a null bitmap under certain
     * circumstances, such as: no logo url found, network/cache error, etc.
     *
     * @param brandedColorScheme The {@link BrandedColorScheme}, used to tint icons.
     */
    public StatusIconResource getSearchEngineLogo(@BrandedColorScheme int brandedColorScheme) {
        if (needToCheckForSearchEnginePromo() || mSearchEngineLogo == null) {
            return getFallbackSearchIcon(brandedColorScheme);
        }
        recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
        return mSearchEngineLogo;
    }

    /** Returns an icon to be shown as a fallback Search icon. */
    public static StatusIconResource getFallbackSearchIcon(
            @BrandedColorScheme int brandedColorScheme) {
        return new StatusIconResource(
                R.drawable.ic_search, ThemeUtils.getThemedToolbarIconTintRes(brandedColorScheme));
    }

    /** Returns an icon to be shown as a fallback Navigation icon. */
    public static StatusIconResource getFallbackNavigationIcon(
            @BrandedColorScheme int brandedColorScheme) {
        return new StatusIconResource(
                R.drawable.ic_globe_24dp,
                ThemeUtils.getThemedToolbarIconTintRes(brandedColorScheme));
    }

    /**
     * Returns whether the search engine promo is complete. Once fetchCheckForSearchEnginePromo()
     * returns false the first time, this method will cache that result as it's presumed we don't
     * need to re-run the promo during the process lifetime.
     */
    @VisibleForTesting
    boolean needToCheckForSearchEnginePromo() {
        if (mNeedToCheckForSearchEnginePromo == null || mNeedToCheckForSearchEnginePromo) {
            mNeedToCheckForSearchEnginePromo = fetchCheckForSearchEnginePromo();
            // getCheckForSearchEnginePromo can fail; if it does, we'll stay in the uncached
            // state and return false.
            if (mNeedToCheckForSearchEnginePromo == null) return false;
        }
        return mNeedToCheckForSearchEnginePromo;
    }

    /**
     * Performs a (potentially expensive) lookup of whether we need to check for a search engine
     * promo. In rare cases this can fail; in these cases it will return null.
     */
    private Boolean fetchCheckForSearchEnginePromo() {
        // LocaleManager#needToCheckForSearchEnginePromo() checks several system features which
        // risk throwing exceptions. See the exception cases below for details.
        try {
            return LocaleManager.getInstance().needToCheckForSearchEnginePromo();
        } catch (SecurityException e) {
            Log.e(TAG, "Can be thrown by a failed IPC, see crbug.com/1027709\n", e);
            return null;
        } catch (RuntimeException e) {
            Log.e(TAG, "Can be thrown if underlying services are dead, see crbug.com/1121602\n", e);
            return null;
        }
    }

    /**
     * Records an event to the search engine logo histogram. See {@link Events} and histograms.xml
     * for more details.
     *
     * @param event The {@link Events} to be reported.
     */
    @VisibleForTesting
    void recordEvent(@Events int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "AndroidSearchEngineLogo.Events", event, Events.MAX);
    }

    /** Set the instance for testing. */
    public static void setInstanceForTesting(SearchEngineUtils instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /*
     * Returns whether the current search provider has Logo.
     */
    boolean doesDefaultSearchEngineHaveLogo() {
        return mDoesDefaultSearchEngineHaveLogo;
    }
}
