// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager.SearchEngineMetadata;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.GURL;

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

        retrieveFavicon(templateUrl, this::setSearchEngineIcon);
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
     * @param fuseboxSessionState optional session state to resolve hint text for specific tools.
     * @return Hint text appropriate for the specific AutocompleteRequestType.
     */
    public String getOmniboxHintText(
            @AutocompleteRequestType int type, @Nullable FuseboxSessionState fuseboxSessionState) {
        if (fuseboxSessionState != null) {
            var input = fuseboxSessionState.getAutocompleteInput();
            if (input != null) {
                GURL url = input.getPageUrl();
                String title = input.getPageTitle();
                if (ContextualTasksUtils.isContextualTasksUrl(url) && !TextUtils.isEmpty(title)) {
                    return title;
                }
            }
        }

        if (TextUtils.isEmpty(mSearchEngineName)) {
            return OmniboxResourceProvider.getString(mContext, R.string.omnibox_empty_hint);
        }

        // TODO(https://crbug.com/492729471): When removing model picker config, do not simply
        // remove the if check below. Instead rethink how our callers invoke this, and try to
        // simplify, potentially by removing request type.
        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            if (ToolModeUtils.isAimRequest(type)) {
                String toolHint = getToolHintFromState(type, fuseboxSessionState);
                if (!TextUtils.isEmpty(toolHint)) {
                    return toolHint;
                }
            }
        }

        @StringRes
        int res =
                switch (type) {
                    case AutocompleteRequestType.AI_MODE ->
                            R.string.omnibox_ai_mode_scope_placeholder_text;
                    case AutocompleteRequestType.IMAGE_GENERATION ->
                            R.string.omnibox_empty_hint_for_image_generation;
                    default ->
                            OmniboxFeatures.sUseAskHintForNtp.getValue()
                                            && mTemplateUrlService.isDefaultSearchEngineGoogle()
                                    ? R.string.omnibox_empty_ask_hint_with_dse_name
                                    : R.string.omnibox_empty_hint_with_dse_name;
                };

        return OmniboxResourceProvider.getString(mContext, res, mSearchEngineName);
    }

    /** Returns the current tool's hint or null if none can be found. */
    private static @Nullable String getToolHintFromState(
            @AutocompleteRequestType int requestType,
            @Nullable FuseboxSessionState fuseboxSessionState) {
        if (fuseboxSessionState == null) return null;

        ComposeboxQueryControllerBridge bridge =
                fuseboxSessionState.getComposeboxQueryControllerBridge();
        if (bridge == null) return null;

        InputState inputState = bridge.getInputStateSupplier().get();
        if (inputState == null) return null;

        // TODO (https://crbug.com/492562651): Passing hasAttachments false here is currently
        // required to correctly function, as the InputState isn't being updated to change request
        // type config for image gen when it has attachments. Unclear if this is correct.
        int activeTool =
                ToolModeUtils.getToolModeForRequestType(requestType, /* hasAttachments= */ false);
        for (ToolConfig config : inputState.toolConfigs) {
            if (config.getToolValue() == activeTool) {
                return config.getHintText();
            }
        }

        return null;
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
        }
    }

    /**
     * Retrieve the favicon for the given TemplateUrl.
     *
     * @param templateUrl The TemplateUrl to retrieve the favicon for.
     * @param callback The callback to receive the StatusIconResource, or null if not found.
     */
    public void retrieveFavicon(
            TemplateUrl templateUrl, Callback<@Nullable StatusIconResource> callback) {
        retrieveFaviconFromBrandedResources(templateUrl, callback);
    }

    private void retrieveFaviconFromBrandedResources(
            TemplateUrl templateUrl, Callback<@Nullable StatusIconResource> callback) {
        // Branded resources are only available on Chrome branded builds.
        if (BuildConfig.IS_CHROME_BRANDED) {
            @Nullable Bitmap bm = templateUrl.getBuiltInSearchEngineIcon();
            if (bm != null) {
                callback.onResult(
                        new StatusIconResource(
                                templateUrl.getFaviconURL().getSpec(), bm, Resources.ID_NULL));
                return;
            }
        }

        retrieveFaviconForStarterPack(templateUrl, callback);
    }

    private void retrieveFaviconForStarterPack(
            TemplateUrl templateUrl, Callback<@Nullable StatusIconResource> callback) {
        @Nullable StatusIconResource icon =
                switch (templateUrl.getStarterPackId()) {
                    case StarterPackId.BOOKMARKS ->
                            new StatusIconResource(R.drawable.ic_star_24dp, 0);
                    case StarterPackId.HISTORY ->
                            new StatusIconResource(R.drawable.ic_history_24dp, 0);
                    case StarterPackId.TABS -> new StatusIconResource(R.drawable.switch_to_tab, 0);
                    case StarterPackId.GEMINI ->
                            new StatusIconResource(R.drawable.ic_spark_4c_16dp, 0);
                    default -> null;
                };
        if (icon != null) {
            callback.onResult(icon);
            return;
        }
        retrieveFaviconFromDefaultResources(templateUrl, callback);
    }

    @VisibleForTesting
    void retrieveFaviconFromDefaultResources(
            TemplateUrl templateUrl, Callback<@Nullable StatusIconResource> callback) {
        if (mTemplateUrlService.getSearchEngineTypeFromTemplateUrl(templateUrl.getKeyword())
                != SearchEngineType.SEARCH_ENGINE_GOOGLE) {
            // Fall back to next source.
            retrieveFaviconFromOriginUrl(templateUrl, callback);
            return;
        }

        callback.onResult(
                new StatusIconResource(R.drawable.ic_logo_googleg_20dp, Resources.ID_NULL));
    }

    private void retrieveFaviconFromOriginUrl(
            TemplateUrl templateUrl, Callback<@Nullable StatusIconResource> callback) {
        var originUrl = new GURL(templateUrl.getURL()).getOrigin();
        boolean willCall =
                mFaviconHelper.getLocalFaviconImageForURL(
                        mProfile,
                        originUrl,
                        mSearchEngineLogoTargetSizePixels,
                        /* fallbackToHost= */ true,
                        (image, iconUrl) -> {
                            if (image == null) {
                                callback.onResult(null);
                            } else {
                                callback.onResult(
                                        new StatusIconResource(
                                                originUrl.getSpec(), image, Resources.ID_NULL));
                            }
                        });

        if (!willCall) {
            callback.onResult(null);
        }
    }

    private void resetFavicon() {
        setSearchEngineIcon(null);
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
            Log.e(TAG, "Can be thrown by a failed IPC, see crbug.com/40660387\n", e);
            return null;
        } catch (RuntimeException e) {
            Log.e(
                    TAG,
                    "Can be thrown if underlying services are dead, see crbug.com/40715590\n",
                    e);
            return null;
        }
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

    /** Returns whether the default search engine is Google. */
    public boolean isDefaultSearchEngineGoogle() {
        return mTemplateUrlService.isDefaultSearchEngineGoogle();
    }
}
