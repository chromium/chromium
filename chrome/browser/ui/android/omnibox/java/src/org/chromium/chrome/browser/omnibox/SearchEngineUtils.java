// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Initializer;
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
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

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
    private final ObserverList<SearchBoxHintTextObserver> mSearchBoxHintTextObservers =
            new ObserverList<>();
    private final ObserverList<SearchEngineIconObserver> mSearchEngineIconObservers =
            new ObserverList<>();
    private @Nullable SearchEngineMetadata mDefaultSearchEngineMetadata;
    private @Nullable Boolean mNeedToCheckForSearchEnginePromo;
    private boolean mDoesDefaultSearchEngineHaveLogo;
    private @Nullable StatusIconResource mFavicon;
    private @Nullable String mSearchEngineName;

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

    @FunctionalInterface
    public interface SearchBoxHintTextObserver {
        /**
         * Invoked when the Search Box hint text changes.
         *
         * @param newHintText the new hint text to apply
         */
        void onSearchBoxHintTextChanged();
    }

    @FunctionalInterface
    public interface SearchEngineIconObserver {
        /**
         * Invoked when the Search Engine icon changes.
         *
         * @param newIcon the new search engine icon to apply
         */
        void onSearchEngineIconChanged(@Nullable StatusIconResource newIcon);
    }

    private SearchEngineUtils(
            Profile profile, FaviconHelper faviconHelper, ImageFetcher imageFetcher) {
        mProfile = profile;
        mIsOffTheRecord = profile.isOffTheRecord();
        mFaviconHelper = faviconHelper;
        mContext = ContextUtils.getApplicationContext();

        mImageFetcher = imageFetcher;

        mSearchEngineLogoTargetSizePixels =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_search_engine_logo_composed_size);

        // Apply safe fallback values.
        setSearchBoxHintText(null);
        resetFavicon();

        mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        mTemplateUrlService.addObserver(this);
        mDefaultSearchEngineMetadata = CachedZeroSuggestionsManager.readSearchEngineMetadata();

        onTemplateURLServiceChanged();
    }

    @VisibleForTesting
    SearchEngineUtils(Profile profile, FaviconHelper faviconHelper) {
        this(
                profile,
                faviconHelper,
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile.getProfileKey(),
                        GlobalDiscardableReferencePool.getReferencePool(),
                        MAX_IMAGE_CACHE_SIZE_BYTES));
    }

    public static SearchEngineUtils createSearchEngineUtilsForTesting(
            Profile profile, FaviconHelper faviconHelper, ImageFetcher imageFetcher) {
        return new SearchEngineUtils(profile, faviconHelper, imageFetcher);
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
        mSearchEngineIconObservers.clear();
        mSearchBoxHintTextObservers.clear();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        mDoesDefaultSearchEngineHaveLogo = mTemplateUrlService.doesDefaultSearchEngineHaveLogo();

        var templateUrl = mTemplateUrlService.getDefaultSearchEngineTemplateUrl();
        if (templateUrl == null) {
            recordEvent(Events.FETCH_FAILED_NULL_URL);
            setSearchBoxHintText(null);
            return;
        }

        if (!TextUtils.isEmpty(templateUrl.getShortName())) {
            setSearchBoxHintText(templateUrl.getShortName());
        } else {
            setSearchBoxHintText(null);
        }

        if (mDefaultSearchEngineMetadata == null
                || !TextUtils.equals(
                        mDefaultSearchEngineMetadata.keyword, templateUrl.getKeyword())) {
            mDefaultSearchEngineMetadata = new SearchEngineMetadata(templateUrl.getKeyword());
            CachedZeroSuggestionsManager.eraseCachedData();
            CachedZeroSuggestionsManager.saveSearchEngineMetadata(mDefaultSearchEngineMetadata);
        }

        retrieveFaviconFromBrandedResources(templateUrl);
    }

    /** Add observer to be notified whenever the Omnibox hint text changes. */
    public void addSearchBoxHintTextObserver(SearchBoxHintTextObserver observer) {
        mSearchBoxHintTextObservers.addObserver(observer);
        observer.onSearchBoxHintTextChanged();
    }

    /** Remove previously registered Omnibox hint text observer. */
    public void removeSearchBoxHintTextObserver(SearchBoxHintTextObserver observer) {
        mSearchBoxHintTextObservers.removeObserver(observer);
    }

    @Initializer
    private void setSearchBoxHintText(@Nullable String engineName) {
        // mSearchBoxHintText may be null when this method is invoked from constructor.
        // This may generate a warning that this field is null. This is fine.
        if (Objects.equals(engineName, mSearchEngineName)) return;
        mSearchEngineName = engineName;
        for (var observer : mSearchBoxHintTextObservers) {
            observer.onSearchBoxHintTextChanged();
        }
    }

    /**
     * Returns the Omnibox Hint Text appropriate for specific AutocompleteRequestType.
     *
     * @param type the type of the AutocompleteRequestType to get the hint text for
     * @return Hint text appropriate for the specific AutocompleteRequestType.
     */
    public String getOmniboxHintText(@AutocompleteRequestType int type) {
        if (TextUtils.isEmpty(mSearchEngineName)) {
            return OmniboxResourceProvider.getString(mContext, R.string.omnibox_empty_hint);
        }

        @StringRes
        int res =
                switch (type) {
                    case AutocompleteRequestType.AI_MODE ->
                            R.string.omnibox_empty_hint_with_dse_name_for_ai_mode;
                    case AutocompleteRequestType.IMAGE_GENERATION ->
                            R.string.omnibox_empty_hint_for_image_generation;
                    default -> R.string.omnibox_empty_hint_with_dse_name;
                };

        return OmniboxResourceProvider.getString(mContext, res, mSearchEngineName);
    }

    /** Add observer to be notified whenever the Search Enigne Icon changes. */
    public void addIconObserver(SearchEngineIconObserver observer) {
        mSearchEngineIconObservers.addObserver(observer);
        observer.onSearchEngineIconChanged(mFavicon);
    }

    /** Remove previously registered Search Engine Icon observer. */
    public void removeIconObserver(SearchEngineIconObserver observer) {
        mSearchEngineIconObservers.removeObserver(observer);
    }

    @VisibleForTesting
    public void setSearchEngineIcon(@Nullable StatusIconResource newIcon) {
        if (Objects.equals(mFavicon, newIcon)) return;
        mFavicon = newIcon;
        for (var observer : mSearchEngineIconObservers) {
            observer.onSearchEngineIconChanged(newIcon);
            recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
        }
    }

    @VisibleForTesting
    void retrieveFaviconFromDefaultResources(TemplateUrl templateUrl) {
        if (!mTemplateUrlService.isDefaultSearchEngineGoogle()) {
            // Fall back to next source.
            recordEvent(Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
            retrieveFaviconFromOriginUrl(templateUrl);
            return;
        }

        setSearchEngineIcon(new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
    }

    private void retrieveFaviconFromBrandedResources(TemplateUrl templateUrl) {
        // Branded resources are only available on Chrome branded builds.
        if (BuildConfig.IS_CHROME_BRANDED
                && OmniboxFeatures.sOmniboxParityRetrieveBuiltInEngineIcon.getValue()) {
            @Nullable Bitmap bm = templateUrl.getBuiltInSearchEngineIcon();
            if (bm != null) {
                onFaviconRetrieveCompleted(templateUrl.getFaviconURL(), bm);
                return;
            }
        }

        retrieveFaviconFromDefaultResources(templateUrl);
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
        setSearchEngineIcon(null);
    }

    private void onFaviconRetrieveCompleted(GURL faviconUrl, Bitmap bitmap) {
        setSearchEngineIcon(new StatusIconResource(faviconUrl.getSpec(), bitmap, 0));
        recordEvent(Events.FETCH_SUCCESS);
    }

    /** Returns whether the search engine logo should be shown. */
    public boolean shouldShowSearchEngineLogo() {
        return !mIsOffTheRecord;
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
}
