// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Collection of shared code for displaying search engine logos. */
public class SearchEngineLogoUtils {
    // Note: shortened to account for the 20 character limit.
    private static final String TAG = "SearchLogoUtils";
    private static final String DUMMY_URL_QUERY = "replace_me";

    private static SearchEngineLogoUtils sInstance;
    // Cached values to prevent duplicate work.
    private static Bitmap sCachedComposedImage;
    private static String sCachedComposedBackgroundLogoUrl;
    private static int sSearchEngineLogoTargetSizePixels;
    private static int sSearchEngineLogoComposedSizePixels;
    private Boolean mNeedToCheckForSearchEnginePromo;

    /**
     * Get the singleton instance of SearchEngineLogoUtils. Avoid using in new code; instead - rely
     * on plumbing supplied instance.
     */
    @Deprecated
    public static SearchEngineLogoUtils getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SearchEngineLogoUtils();
        }
        return sInstance;
    }

    // Lazy initialization for native-bound dependencies.
    private FaviconHelper mFaviconHelper;
    private boolean mDoesSearchProviderHaveLogo;

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
    SearchEngineLogoUtils() {}

    /**
     * Encapsulates the check for if the search engine logo should be shown.
     *
     * @param isOffTheRecord True if the user is currently using an incognito tab.
     * @return True if we should show the search engine logo.
     */
    public boolean shouldShowSearchEngineLogo(boolean isOffTheRecord) {
        return !isOffTheRecord;
    }

    /**
     * @param templateUrlService The TemplateUrlService to use to derive the logo url.
     * @return the search URL of the current DSE or null if one cannot be found.
     */
    @VisibleForTesting
    @Nullable
    String getSearchLogoUrl(@Nullable TemplateUrlService templateUrlService) {
        if (templateUrlService == null) return null;

        String logoUrlWithPath = templateUrlService.getUrlForSearchQuery(DUMMY_URL_QUERY);
        if (logoUrlWithPath == null || !UrlUtilities.isHttpOrHttps(logoUrlWithPath)) {
            return logoUrlWithPath;
        }

        // The extra "/" would be added by GURL anyway.
        return UrlUtilities.stripPath(logoUrlWithPath) + "/";
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The size that the logo favicon should be.
     */
    private int getSearchEngineLogoSizePixels(@NonNull Resources resources) {
        if (sSearchEngineLogoTargetSizePixels == 0) {
            sSearchEngineLogoTargetSizePixels =
                    resources.getDimensionPixelSize(
                            R.dimen.omnibox_search_engine_logo_favicon_size);
        }

        return sSearchEngineLogoTargetSizePixels;
    }

    /**
     * Get the search engine logo favicon. This can return a null bitmap under certain
     * circumstances, such as: no logo url found, network/cache error, etc.
     *
     * @param resources Provides access to Android resources.
     * @param brandedColorScheme The {@link BrandedColorScheme}, used to tint icons.
     * @param profile The current profile. When null, falls back to locally-provided icons.
     * @param templateUrlService The current templateUrlService. When null, falls back to
     *     locally-provided icons.
     */
    public Promise<StatusIconResource> getSearchEngineLogo(
            @NonNull Resources resources,
            @BrandedColorScheme int brandedColorScheme,
            @Nullable Profile profile,
            @Nullable TemplateUrlService templateUrlService) {
        onDefaultSearchEngineChanged(templateUrlService);

        // In the following cases, we fallback to the search loupe:
        // - Either of the nullable dependencies are null.
        // - We still need to check for the search engine promo, which happens in rare cases when
        //   the search engine promo needed to be shown but wasn't for some reason.
        // If TemplateUrlService is available and the default search engine is Google,
        // then we serve the Google icon we have locally.
        // Otherwise, the search engine is non-Google and we go to the network to fetch it.
        if (profile == null || templateUrlService == null || needToCheckForSearchEnginePromo()) {
            return Promise.fulfilled(getSearchLoupeResource(brandedColorScheme));
        } else if (templateUrlService.isDefaultSearchEngineGoogle()) {
            return Promise.fulfilled(new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
        }

        // If all of the nullable dependencies are present and the search engine is non-Google,
        // then go to the network to fetch the icon.
        recordEvent(Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();

        String logoUrl = getSearchLogoUrl(templateUrlService);
        if (logoUrl == null) {
            recordEvent(Events.FETCH_FAILED_NULL_URL);
            return Promise.fulfilled(getSearchLoupeResource(brandedColorScheme));
        }

        // Return a cached copy if it's available.
        if (sCachedComposedImage != null && sCachedComposedBackgroundLogoUrl.equals(logoUrl)) {
            recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
            return Promise.fulfilled(new StatusIconResource(logoUrl, sCachedComposedImage, 0));
        }

        Promise<StatusIconResource> promise = new Promise<>();
        final int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        boolean willCallbackBeCalled =
                mFaviconHelper.getLocalFaviconImageForURL(
                        profile,
                        new GURL(logoUrl),
                        logoSizePixels,
                        (image, iconUrl) -> {
                            if (image == null) {
                                promise.fulfill(getSearchLoupeResource(brandedColorScheme));
                                recordEvent(Events.FETCH_FAILED_RETURNED_BITMAP_NULL);
                                return;
                            }

                            processReturnedLogo(logoUrl, image, resources, promise);
                            recordEvent(Events.FETCH_SUCCESS);
                        });
        if (!willCallbackBeCalled) {
            promise.fulfill(getSearchLoupeResource(brandedColorScheme));
            recordEvent(Events.FETCH_FAILED_FAVICON_HELPER_ERROR);
        }
        return promise;
    }

    @VisibleForTesting
    StatusIconResource getSearchLoupeResource(@BrandedColorScheme int brandedColorScheme) {
        return new StatusIconResource(
                R.drawable.ic_search, ThemeUtils.getThemedToolbarIconTintRes(brandedColorScheme));
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
     * Process the image returned from a network fetch or cache hit. Reduces the size of the
     * retrieved favicon to reduce memory footprint.
     *
     * @param logoUrl The url for the given logo.
     * @param image The logo to process.
     * @param resources Android resources object used to access dimensions.
     * @param promise The promise encapsulating the processed logo.
     */
    private void processReturnedLogo(
            String logoUrl,
            Bitmap image,
            Resources resources,
            Promise<StatusIconResource> promise) {
        // Scale the logo up to the desired size.
        int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        Bitmap scaledIcon = Bitmap.createScaledBitmap(image, logoSizePixels, logoSizePixels, true);

        sCachedComposedImage = image;
        sCachedComposedBackgroundLogoUrl = logoUrl;

        promise.fulfill(new StatusIconResource(logoUrl, sCachedComposedImage, 0));
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

    /** Set the favicon helper for testing. */
    @VisibleForTesting
    void setFaviconHelperForTesting(FaviconHelper faviconHelper) {
        var oldValue = mFaviconHelper;
        mFaviconHelper = faviconHelper;
        ResettersForTesting.register(() -> mFaviconHelper = oldValue);
    }

    /** Set the instance for testing. */
    @VisibleForTesting
    static void setInstanceForTesting(SearchEngineLogoUtils instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    /** Reset the cache values for testing. */
    @VisibleForTesting
    static void resetForTesting() {
        sInstance = null;
        sCachedComposedImage = null;
        sCachedComposedBackgroundLogoUrl = null;
    }

    /*
     * Changes the status of whether the current search provider has the logo.
     */
    void onDefaultSearchEngineChanged(TemplateUrlService templateUrlService) {
        if (templateUrlService != null) {
            mDoesSearchProviderHaveLogo = templateUrlService.doesDefaultSearchEngineHaveLogo();
        }
    }

    /*
     * Returns whether the current search provider is Google.
     */
    boolean isDefaultSearchEngineGoogle() {
        return mDoesSearchProviderHaveLogo;
    }
}
