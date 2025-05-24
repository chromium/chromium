// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.SearchEngineMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Common Default Search Engine functions. */
@NullMarked
public class SearchEngineUtils implements Destroyable, TemplateUrlServiceObserver {
    private static final int MAX_IMAGE_CACHE_SIZE_BYTES = 4096;
    private static final String TAG = "DSEUtils";
    private static final ProfileKeyedMap<SearchEngineUtils> sProfileKeyedUtils =
            ProfileKeyedMap.createMapOfDestroyables();
    private static @Nullable SearchEngineUtils sInstanceForTesting;

    private final Context mContext;
    private final Profile mProfile;
    private final boolean mIsOffTheRecord;
    private final TemplateUrlService mTemplateUrlService;
    private final FaviconHelper mFaviconHelper;
    private final ImageFetcher mImageFetcher;
    private final int mSearchEngineLogoTargetSizePixels;
    private @Nullable SearchEngineMetadata mDefaultSearchEngineMetadata;
    private @Nullable Boolean mNeedToCheckForSearchEnginePromo;
    private boolean mDoesDefaultSearchEngineHaveLogo;
    private @Nullable StatusIconResource mFavicon;
    private String mSearchBoxHintText;

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
        mContext = ContextUtils.getApplicationContext();

        mImageFetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool(),
                        MAX_IMAGE_CACHE_SIZE_BYTES);

        mSearchEngineLogoTargetSizePixels =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_search_engine_logo_favicon_size);

        // Apply safe fallback values.
        mSearchBoxHintText =
                OmniboxResourceProvider.getString(mContext, R.string.omnibox_empty_hint);
        resetFavicon();

        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this);
        mDefaultSearchEngineMetadata = CachedZeroSuggestionsManager.readSearchEngineMetadata();

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
        mFaviconHelper.destroy();
        mImageFetcher.destroy();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        mDoesDefaultSearchEngineHaveLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();
        mSearchBoxHintText =
                OmniboxResourceProvider.getString(mContext, R.string.omnibox_empty_hint);

        var templateUrl = mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
        if (templateUrl == null) {
            recordEvent(Events.FETCH_FAILED_NULL_URL);
            return;
        }

        if (OmniboxFeatures.sOmniboxMobileParityUpdate.isEnabled()
                && !TextUtils.isEmpty(templateUrl.getShortName())) {
            mSearchBoxHintText =
                    OmniboxResourceProvider.getString(
                            mContext,
                            R.string.omnibox_empty_hint_with_dse_name,
                            templateUrl.getShortName());
        }

        if (mDefaultSearchEngineMetadata == null
                || !TextUtils.equals(
                        mDefaultSearchEngineMetadata.keyword, templateUrl.getKeyword())) {
            mDefaultSearchEngineMetadata = new SearchEngineMetadata(templateUrl.getKeyword());
            CachedZeroSuggestionsManager.eraseCachedData();
            CachedZeroSuggestionsManager.saveSearchEngineMetadata(mDefaultSearchEngineMetadata);
        }

        retrieveFavicon(templateUrl);
    }

    @VisibleForTesting
    void retrieveFavicon(TemplateUrl templateUrl) {
        if (!mTemplateUrlService.isDefaultSearchEngineGoogle()) {
            // Fall back to next source.
            recordEvent(Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
            retrieveFaviconFromFaviconUrl(templateUrl);
            return;
        }

        mFavicon = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
    }

    private void retrieveFaviconFromFaviconUrl(TemplateUrl templateUrl) {
        var faviconUrl = templateUrl.getFaviconURL();
        if (!OmniboxFeatures.sOmniboxParityRetrieveTrueFavicon.getValue()
                || GURL.isEmptyOrInvalid(faviconUrl)) {
            // Fall back to next source.
            retrieveFaviconFromOriginUrl(templateUrl);
            return;
        }

        ImageFetcher.Params params =
                ImageFetcher.Params.create(faviconUrl, ImageFetcher.OMNIBOX_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(
                params,
                bitmap -> {
                    if (bitmap == null) {
                        retrieveFaviconFromOriginUrl(templateUrl);
                    } else {
                        onFaviconRetrieveCompleted(faviconUrl, bitmap);
                    }
                });
    }

    private void retrieveFaviconFromOriginUrl(TemplateUrl templateUrl) {
        var originUrl = new GURL(templateUrl.getURL()).getOrigin();
        boolean willCall =
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile,
                        originUrl,
                        mSearchEngineLogoTargetSizePixels,
                        (image, iconUrl) -> {
                            if (image == null) {
                                recordEvent(Events.FETCH_FAILED_RETURNED_BITMAP_NULL);
                                resetFavicon();
                            } else {
                                onFaviconRetrieveCompleted(originUrl, image);
                            }
                        });

        if (!willCall) {
            recordEvent(Events.FETCH_FAILED_FAVICON_HELPER_ERROR);
            resetFavicon();
        }
    }

    private void resetFavicon() {
        mFavicon = null;
    }

    private void onFaviconRetrieveCompleted(GURL faviconUrl, Bitmap bitmap) {
        mFavicon = new StatusIconResource(faviconUrl.getSpec(), bitmap, 0);
        recordEvent(Events.FETCH_SUCCESS);
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
        if (needToCheckForSearchEnginePromo() || mFavicon == null) {
            return getFallbackSearchIcon(brandedColorScheme);
        }
        recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
        return mFavicon;
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
    private @Nullable Boolean fetchCheckForSearchEnginePromo() {
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
    static void recordEvent(@Events int event) {
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
    public boolean doesDefaultSearchEngineHaveLogo() {
        return mDoesDefaultSearchEngineHaveLogo;
    }

    /** Returns the standardized Omnibox hint text for the current Search Engine. */
    public String getSearchBoxHintText() {
        return mSearchBoxHintText;
    }
}
