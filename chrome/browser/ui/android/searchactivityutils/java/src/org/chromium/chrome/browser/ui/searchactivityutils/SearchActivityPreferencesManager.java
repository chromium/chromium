// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.Arrays;

/**
 * Facilitates access to and updates of the cached SearchActivityPreferences.
 */
public class SearchActivityPreferencesManager {
    /** Data-only class representiing current SearchActivity preferences. */
    public static final class SearchActivityPreferences {
        /** Name of the Default Search Engine. */
        public final @Nullable String searchEngineName;
        /** URL of the Default Search Engine. */
        public final @Nullable String searchEngineUrl;
        /** Whether Voice Search functionality is available. */
        public final boolean voiceSearchAvailable;
        /** Whether Google Lens functionality is available. */
        public final boolean googleLensAvailable;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        public SearchActivityPreferences(@Nullable String searchEngineName,
                @Nullable String searchEngineUrl, boolean voiceSearchAvailable,
                boolean googleLensAvailable) {
            this.searchEngineName = searchEngineName;
            this.searchEngineUrl = searchEngineUrl;
            this.voiceSearchAvailable = voiceSearchAvailable;
            this.googleLensAvailable = googleLensAvailable;
        }

        @Override
        public boolean equals(Object otherObj) {
            if (otherObj == this) return true;
            if (!(otherObj instanceof SearchActivityPreferences)) return false;

            SearchActivityPreferences other = (SearchActivityPreferences) otherObj;
            return voiceSearchAvailable == other.voiceSearchAvailable
                    && googleLensAvailable == other.googleLensAvailable
                    && TextUtils.equals(searchEngineName, other.searchEngineName)
                    && TextUtils.equals(searchEngineUrl, other.searchEngineUrl);
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(new Object[] {
                    searchEngineName, searchEngineUrl, voiceSearchAvailable, googleLensAvailable});
        }
    }

    /** Monitors the TemplateUrlService for changes, updating the widget when necessary. */
    private final class SearchActivityTemplateUrlServiceObserver
            implements LoadListener, TemplateUrlServiceObserver {
        @Override
        public void onTemplateUrlServiceLoaded() {
            TemplateUrlServiceFactory.get().unregisterLoadListener(this);
            updateCachedValues();
        }

        @Override
        public void onTemplateURLServiceChanged() {
            updateCachedValues();
        }
    }

    /**
     * The default/fallback value describing Voice Search availability.
     * This value will be used unless voice search is detected as unavailable.
     */
    private static final boolean DEFAULT_VOICE_SEARCH_AVAILABILITY = true;
    /**
     * The default/fallback value describing Gooogle Lens availability.
     * This value will be used unless voice search is detected as available.
     */
    private static final boolean DEFAULT_GOOGLE_LENS_AVAILABILITY = false;

    private static @Nullable SearchActivityPreferencesManager sInstance;
    private final @NonNull ObserverList<Consumer<SearchActivityPreferences>> mObservers =
            new ObserverList<>();
    private final @NonNull SearchActivityTemplateUrlServiceObserver mTemplateUrlServiceObserver =
            new SearchActivityTemplateUrlServiceObserver();
    private @NonNull SearchActivityPreferences mCurrentlyLoadedPreferences;

    /**
     * Initialize instance of SearchActivityPreferencesManager.
     * Note that the class operates as a singleton, because it may - and will be invoked from
     * multiple independent contexts.
     */
    private SearchActivityPreferencesManager() {}

    /**
     * @return The instance of the SearchActivityPreferencesManager singleton.
     */
    public static SearchActivityPreferencesManager get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SearchActivityPreferencesManager();
            readCachedValues();
        }
        return sInstance;
    }

    /**
     * Returns current knowh SharedActivityPreferences values.
     */
    public static @NonNull SearchActivityPreferences getCurrent() {
        return get().mCurrentlyLoadedPreferences;
    }

    /**
     * Fetch previously cached Search Widget details, if any.
     * When no previous values were found, the code will initialize values to safe defaults.
     *
     * If stored values are different than current values, the update will be propagated to
     * registered listeners.
     */
    public static void readCachedValues() {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        SearchActivityPreferences prefs = new SearchActivityPreferences(
                manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, null),
                manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_URL, null),
                manager.readBoolean(
                        SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE, DEFAULT_VOICE_SEARCH_AVAILABILITY),
                manager.readBoolean(
                        SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE, DEFAULT_GOOGLE_LENS_AVAILABILITY));
        setCurrentlyLoadedPreferences(prefs, false);
    }

    /**
     * Update availability of Search Activity features.
     * The updated information will be retained to disk for possible use at a later time.
     * If new values are different than current values, the update will be propagated to registered
     * listeners.
     *
     * @param voiceSearchAvailable Whether VoiceSearch is available.
     */
    public static void updateCachedValues(boolean voiceSearchAvailable) {
        assert LibraryLoader.getInstance().isInitialized();
        Pair<String, String> defaultSearchEngine = getDefaultSearchEngine();
        String searchEngineName = null;
        String searchEngineUrl = null;
        if (defaultSearchEngine != null) {
            searchEngineName = defaultSearchEngine.first;
            searchEngineUrl = defaultSearchEngine.second;
        }

        // TODO(crbug/1213541): Update the googleLensAvailable parameter with appropriate call to
        // LensSdkUtils. Until we can get the detection completed fall back to safe defaults.
        SearchActivityPreferences prefs = new SearchActivityPreferences(searchEngineName,
                searchEngineUrl, voiceSearchAvailable,
                /* googleLensAvailable=*/false);
        setCurrentlyLoadedPreferences(prefs, true);
    }

    /**
     * Update availability of Search Activity features.
     * Assumes the VoiceSearch availability remains unchanged.
     * The updated information will be retained to disk for possible use at a later time.
     * If new values are different than current values, the update will be propagated to registered
     * listeners.
     */
    public static void updateCachedValues() {
        SearchActivityPreferencesManager self = get();
        updateCachedValues(self.mCurrentlyLoadedPreferences.voiceSearchAvailable);
    }

    /**
     * Clear all cached preferences.
     * If reset values are different than current values, the update will be propagated to
     * registered listeners.
     */
    public static void resetCachedValues() {
        SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        manager.removeKey(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME);
        manager.removeKey(SEARCH_WIDGET_SEARCH_ENGINE_URL);
        manager.removeKey(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE);
        manager.removeKey(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE);
        setCurrentlyLoadedPreferences(
                new SearchActivityPreferences(null, null, DEFAULT_VOICE_SEARCH_AVAILABILITY,
                        DEFAULT_GOOGLE_LENS_AVAILABILITY),
                false);
    }

    /**
     * Specify current SearchActivityPreferences values.
     * If the supplied values are different than current values, the update will be propagated to
     * registered listeners.
     *
     * @param prefs Current preferences.
     * @param updateStorage Whether to update on-disk cache.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void setCurrentlyLoadedPreferences(
            @NonNull SearchActivityPreferences prefs, boolean updateStorage) {
        SearchActivityPreferencesManager self = get();
        if (prefs.equals(self.mCurrentlyLoadedPreferences)) return;
        self.mCurrentlyLoadedPreferences = prefs;

        if (updateStorage) {
            SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
            manager.writeString(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, prefs.searchEngineName);
            manager.writeString(SEARCH_WIDGET_SEARCH_ENGINE_URL, prefs.searchEngineUrl);
            manager.writeBoolean(
                    SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE, prefs.voiceSearchAvailable);
            manager.writeBoolean(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE, prefs.googleLensAvailable);
        }

        // Notify all listeners about update.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            for (Consumer<SearchActivityPreferences> observer : self.mObservers) {
                observer.accept(prefs);
            }
        });
    }

    /**
     * Add a new preference change observer.
     * This method guarantees that the newly added observer will instantly receive information about
     * current preferences.
     *
     * @param observer The observer to be added.
     */
    public static void addObserver(@NonNull Consumer<SearchActivityPreferences> observer) {
        ThreadUtils.assertOnUiThread();
        assert observer != null : "SearchActivityPreferences observer must be valid.";
        SearchActivityPreferencesManager self = get();
        if (!self.mObservers.hasObserver(observer)) {
            self.mObservers.addObserver(observer);
            observer.accept(self.mCurrentlyLoadedPreferences);
        }
    }

    /**
     * Creates the observer that will monitor for search engine changes.
     * The native library and the browser process must have been fully loaded before calling this.
     */
    public static void onNativeLibraryReady() {
        assert LibraryLoader.getInstance().isInitialized();
        SearchActivityPreferencesManager self = get();
        TemplateUrlService service = TemplateUrlServiceFactory.get();
        service.registerLoadListener(self.mTemplateUrlServiceObserver);
        service.addObserver(self.mTemplateUrlServiceObserver);
        if (!service.isLoaded()) {
            service.load();
        }
    }

    /**
     * Retrieve the current search engine name and URL, or null if not possible at this time.
     * Requires that the Native libraries are initialized.
     *
     * @return The currently selected search engine name and URL, if available, otherwise null is
     *         returned.
     */
    private static @Nullable Pair<String, String> getDefaultSearchEngine() {
        assert LibraryLoader.getInstance().isInitialized();
        // Getting an instance of the TemplateUrlService requires that the native library be
        // loaded, but the TemplateUrlService also itself needs to be initialized.
        TemplateUrlService service = TemplateUrlServiceFactory.get();
        assert service.isLoaded() : "TemplateUrlServiceFactory is not ready yet.";

        // Update the URL that we show for zero-suggest.
        TemplateUrl dseTemplateUrl = service.getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl == null) return null;

        String searchEngineUrl =
                service.getSearchEngineUrlFromTemplateUrl(dseTemplateUrl.getKeyword());
        UrlBarData urlBarData = UrlBarData.forUrl(searchEngineUrl);

        return new Pair<>(dseTemplateUrl.getShortName(),
                urlBarData.displayText
                        .subSequence(urlBarData.originStartIndex, urlBarData.originEndIndex)
                        .toString());
    }

    /**
     * Reset the global instance of the SearchActivityPreferencesManager for the purpose of testing.
     */
    static void resetForTesting() {
        sInstance = null;
    }
}
