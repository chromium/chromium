// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Collection of shared code for displaying search engine logos.
 */
public class SearchEngineLogoUtils {
    // Note: shortened to account for the 20 character limit.
    private static final String TAG = "SearchLogoUtils";
    private static final String DUMMY_URL_QUERY = "replace_me";

    private static SearchEngineLogoUtils sInstance;
    // Cached values to prevent duplicate work.
    private static Bitmap sCachedComposedBackground;
    private static String sCachedComposedBackgroundLogoUrl;
    private static int sSearchEngineLogoTargetSizePixels;
    private static int sSearchEngineLogoComposedSizePixels;
    private Boolean mNeedToCheckForSearchEnginePromo;

    /** Get the singleton instance of SearchEngineLogoUtils */
    public static SearchEngineLogoUtils getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SearchEngineLogoUtils();
        }
        return sInstance;
    }

    // Lazy initialization for native-bound dependencies.
    private FaviconHelper mFaviconHelper;
    private RoundedIconGenerator mRoundedIconGenerator;

    /**
     * AndroidSearchEngineLogoEvents defined in tools/metrics/histograms/enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({Events.FETCH_NON_GOOGLE_LOGO_REQUEST, Events.FETCH_FAILED_NULL_URL,
            Events.FETCH_FAILED_FAVICON_HELPER_ERROR, Events.FETCH_FAILED_RETURNED_BITMAP_NULL,
            Events.FETCH_SUCCESS_CACHE_HIT, Events.FETCH_SUCCESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Events {
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
     * @param isOffTheRecord True if the user is currently using an incognito tab.
     * @return True if we should show the search engine logo.
     */
    public boolean shouldShowSearchEngineLogo(boolean isOffTheRecord) {
        return !isOffTheRecord;
    }

    /**
     * @param templateUrlService The TemplateUrlService to use to derive the logo url.
     * Returns the search URL of the current DSE or null if one cannot be found.
     */
    @Nullable
    public String getSearchLogoUrl(@Nullable TemplateUrlService templateUrlService) {
        if (templateUrlService == null) return null;

        String logoUrlWithPath = templateUrlService.getUrlForSearchQuery(DUMMY_URL_QUERY);
        if (logoUrlWithPath == null || !UrlUtilities.isHttpOrHttps(logoUrlWithPath)) {
            return logoUrlWithPath;
        }

        // The extra "/" would be added by GURL anyway and is required for ShadowGURL to work
        // correctly in unit tests.
        return UrlUtilities.stripPath(logoUrlWithPath) + "/";
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The size that the logo favicon should be.
     */
    public int getSearchEngineLogoSizePixels(@NonNull Resources resources) {
        if (sSearchEngineLogoTargetSizePixels == 0) {
            sSearchEngineLogoTargetSizePixels = resources.getDimensionPixelSize(
                    R.dimen.omnibox_search_engine_logo_favicon_size);
        }

        return sSearchEngineLogoTargetSizePixels;
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The total size the logo will be on screen.
     */
    public static int getSearchEngineLogoComposedSizePixels(@NonNull Resources resources) {
        if (sSearchEngineLogoComposedSizePixels == 0) {
            sSearchEngineLogoComposedSizePixels = resources.getDimensionPixelSize(
                    R.dimen.omnibox_search_engine_logo_composed_size);
        }

        return sSearchEngineLogoComposedSizePixels;
    }

    /**
     * Get the search engine logo favicon. This can return a null bitmap under certain
     * circumstances, such as: no logo url found, network/cache error, etc.
     *
     * @param resources Provides access to Android resources.
     * @param brandedColorScheme The {@link BrandedColorScheme}, used to tint icons.
     * @param profile The current profile. When null, falls back to locally-provided icons.
     * @param templateUrlService The current templateUrlService. When null, falls back to
     *         locally-provided icons.
     */
    public Promise<StatusIconResource> getSearchEngineLogo(@NonNull Resources resources,
            @BrandedColorScheme int brandedColorScheme, @Nullable Profile profile,
            @Nullable TemplateUrlService templateUrlService) {
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
        if (sCachedComposedBackground != null && sCachedComposedBackgroundLogoUrl.equals(logoUrl)) {
            recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
            return Promise.fulfilled(new StatusIconResource(logoUrl, sCachedComposedBackground, 0));
        }

        Promise<StatusIconResource> promise = new Promise<>();
        final int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        boolean willCallbackBeCalled = mFaviconHelper.getLocalFaviconImageForURL(
                profile, new GURL(logoUrl), logoSizePixels, (image, iconUrl) -> {
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
     * Process the image returned from a network fetch or cache hit. This method processes the logo
     * to make it eligible for display. The logo is resized to ensure it will fill the required
     * size. This is done because the icon returned from native could be a different size. If the
     * rounded edges variant is active, then a smaller icon is downloaded and drawn on top of a
     * circle background. This looks better and also has more predictable behavior than rounding the
     * edges of the full size icon. The circle background is a solid color made up of the result
     * from a call to getMostCommonEdgeColor(...).
     * @param logoUrl The url for the given logo.
     * @param image The logo to process.
     * @param resources Android resources object used to access dimensions.
     * @param promise The promise encapsulating the processed logo.
     */
    private void processReturnedLogo(String logoUrl, Bitmap image, Resources resources,
            Promise<StatusIconResource> promise) {
        // Scale the logo up to the desired size.
        int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        Bitmap scaledIcon =
                Bitmap.createScaledBitmap(image, getSearchEngineLogoSizePixels(resources),
                        getSearchEngineLogoSizePixels(resources), true);

        int composedSizePixels = getSearchEngineLogoComposedSizePixels(resources);
        if (mRoundedIconGenerator == null) {
            mRoundedIconGenerator = new RoundedIconGenerator(composedSizePixels, composedSizePixels,
                    composedSizePixels, Color.TRANSPARENT, 0);
        }
        int color = (image.getWidth() == 0 || image.getHeight() == 0)
                ? Color.TRANSPARENT
                : getMostCommonEdgeColor(image);
        mRoundedIconGenerator.setBackgroundColor(color);

        // Generate a rounded background with no text.
        Bitmap composedIcon = mRoundedIconGenerator.generateIconForText("");
        Canvas canvas = new Canvas(composedIcon);
        // Draw the logo in the middle of the generated background.
        int dx = (composedSizePixels - logoSizePixels) / 2;
        canvas.drawBitmap(scaledIcon, dx, dx, null);

        // Cache the result icon to reduce future work.
        sCachedComposedBackground = composedIcon;
        sCachedComposedBackgroundLogoUrl = logoUrl;

        promise.fulfill(new StatusIconResource(logoUrl, sCachedComposedBackground, 0));
    }

    /**
     * Samples the edges of given bitmap and returns the most common color.
     * @param icon Bitmap to be sampled.
     */
    @VisibleForTesting
    int getMostCommonEdgeColor(Bitmap icon) {
        Map<Integer, Integer> colorCount = new HashMap<>();
        for (int i = 0; i < icon.getWidth(); i++) {
            // top edge
            int color = icon.getPixel(i, 0);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // bottom edge
            color = icon.getPixel(i, icon.getHeight() - 1);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // Measure the lateral edges offset by 1 on each side.
            if (i > 0 && i < icon.getWidth() - 1) {
                // left edge
                color = icon.getPixel(0, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);

                // right edge
                color = icon.getPixel(icon.getWidth() - 1, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);
            }
        }

        // Find the most common color out of the map.
        int maxKey = Color.TRANSPARENT;
        int maxVal = -1;
        for (int color : colorCount.keySet()) {
            int count = colorCount.get(color);
            if (count > maxVal) {
                maxKey = color;
                maxVal = count;
            }
        }
        assert maxVal > -1;

        return maxKey;
    }

    /**
     * Records an event to the search engine logo histogram. See {@link Events} and histograms.xml
     * for more details.
     * @param event The {@link Events} to be reported.
     */
    @VisibleForTesting
    void recordEvent(@Events int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "AndroidSearchEngineLogo.Events", event, Events.MAX);
    }

    /** Set the favicon helper for testing. */
    void setFaviconHelperForTesting(FaviconHelper faviconHelper) {
        mFaviconHelper = faviconHelper;
    }

    /** Set the RoundedIconGenerator for testing. */
    void setRoundedIconGeneratorForTesting(RoundedIconGenerator roundedIconGenerator) {
        mRoundedIconGenerator = roundedIconGenerator;
    }

    /** Set the instance for testing. */
    static void setInstanceForTesting(SearchEngineLogoUtils instance) {
        sInstance = instance;
    }

    /** Reset the cache values for testing. */
    static void resetForTesting() {
        sInstance = null;
        sCachedComposedBackground = null;
        sCachedComposedBackgroundLogoUrl = null;
    }
}
