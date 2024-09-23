// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensQueryParams;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.function.Consumer;

/** Facilitates access to and updates of the cached SearchActivityPreferences. */
public class SearchActivityPreferencesManager implements LoadListener, TemplateUrlServiceObserver {
    /** Data-only class representiing current SearchActivity preferences. */
    public static final class SearchActivityPreferences {
        /** Name of the Default Search Engine. */
        public final @Nullable String searchEngineName;

        /** URL of the Default Search Engine. */
        public final @NonNull GURL searchEngineUrl;

        /** Whether Voice Search functionality is available. */
        public final boolean voiceSearchAvailable;

        /** Whether Google Lens functionality is available. */
        public final boolean googleLensAvailable;

        /** Whether Incognito browsing functionality is available. */
        public final boolean incognitoAvailable;

        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        public SearchActivityPreferences(
                @Nullable String searchEngineName,
                @Nullable GURL searchEngineUrl,
                boolean voiceSearchAvailable,
                boolean googleLensAvailable,
                boolean incognitoAvailable) {
            this.searchEngineName = searchEngineName;
            this.searchEngineUrl = searchEngineUrl != null ? searchEngineUrl : GURL.emptyGURL();
            this.voiceSearchAvailable = voiceSearchAvailable;
            this.googleLensAvailable = googleLensAvailable;
            this.incognitoAvailable = incognitoAvailable;
        }

        @Override
        public boolean equals(Object otherObj) {
            if (otherObj == this) return true;
            if (!(otherObj instanceof SearchActivityPreferences)) return false;

            SearchActivityPreferences other = (SearchActivityPreferences) otherObj;
            return voiceSearchAvailable == other.voiceSearchAvailable
                    && googleLensAvailable == other.googleLensAvailable
                    && incognitoAvailable == other.incognitoAvailable
                    && TextUtils.equals(searchEngineName, other.searchEngineName)
                    && searchEngineUrl.equals(other.searchEngineUrl);
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(
                    new Object[] {
                        searchEngineName,
                        searchEngineUrl,
                        voiceSearchAvailable,
                        googleLensAvailable,
                        incognitoAvailable
                    });
        }
    }

    /** The default/fallback value describing Voice Search availability. */
    private static final boolean DEFAULT_VOICE_SEARCH_AVAILABILITY = true;

    /** The default/fallback value describing Gooogle Lens availability. */
    private static final boolean DEFAULT_GOOGLE_LENS_AVAILABILITY = false;

    /** The default/fallback value describing Incognito browsing availability. */
    private static final boolean DEFAULT_INCOGNITO_AVAILABILITY = true;

    private static @Nullable SearchActivityPreferencesManager sInstance;
    private final @NonNull ObserverList<Consumer<SearchActivityPreferences>> mObservers =
            new ObserverList<>();
    private @NonNull SearchActivityPreferences mCurrentlyLoadedPreferences;

    /**
     * Initialize instance of SearchActivityPreferencesManager. Note that the class operates as a
     * singleton, because it may - and will be invoked from multiple independent contexts.
     */
    private SearchActivityPreferencesManager() {}

    /**
     * @return The instance of the SearchActivityPreferencesManager singleton.
     */
    public static SearchActivityPreferencesManager get() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SearchActivityPreferencesManager();
            initializeFromCache();
        }
        return sInstance;
    }

    /** Returns current knowh SharedActivityPreferences values. */
    public static @NonNull SearchActivityPreferences getCurrent() {
        return get().mCurrentlyLoadedPreferences;
    }

    /**
     * Fetch previously cached Search Widget details, if any. When no previous values were found,
     * the code will initialize values to safe defaults.
     *
     * <p>If stored values are different than current values, the update will be propagated to
     * registered listeners.
     */
    private static void initializeFromCache() {
        SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        String encodedUrl = manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_URL, null);

        boolean shouldUpdateStorageToSaveSerializedGurl = false;
        GURL url = GURL.emptyGURL();
        if (!TextUtils.isEmpty(encodedUrl)) {
            url = GURL.deserialize(encodedUrl);
            // Deserializing may fail if the URL is not a serialized GURL.
            if (url.isEmpty()) {
                // This will be slow once, as it will attempt to initialize part of native library.
                url = new GURL(encodedUrl);
                shouldUpdateStorageToSaveSerializedGurl = true;
            }
        }

        setCurrentlyLoadedPreferences(
                new SearchActivityPreferences(
                        manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, null),
                        url,
                        manager.readBoolean(
                                SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE,
                                DEFAULT_VOICE_SEARCH_AVAILABILITY),
                        manager.readBoolean(
                                SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE,
                                DEFAULT_GOOGLE_LENS_AVAILABILITY),
                        manager.readBoolean(
                                SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE,
                                DEFAULT_INCOGNITO_AVAILABILITY)),
                shouldUpdateStorageToSaveSerializedGurl);
    }

    /**
     * Clear all cached preferences. If reset values are different than current values, the update
     * will be propagated to registered listeners.
     */
    public static void resetCachedValues() {
        SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        manager.removeKey(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME);
        manager.removeKey(SEARCH_WIDGET_SEARCH_ENGINE_URL);
        manager.removeKey(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE);
        manager.removeKey(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE);
        manager.removeKey(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE);
        initializeFromCache();
    }

    /**
     * Specify current SearchActivityPreferences values. If the supplied values are different than
     * current values, the update will be propagated to registered listeners.
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

        // Notify all listeners about update.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Note: it takes about 6.5ms to update a single property on debug-enabled
                    // builds.
                    if (updateStorage) {
                        SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
                        manager.writeString(
                                SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, prefs.searchEngineName);
                        manager.writeString(
                                SEARCH_WIDGET_SEARCH_ENGINE_URL, prefs.searchEngineUrl.serialize());
                        manager.writeBoolean(
                                SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE,
                                prefs.voiceSearchAvailable);
                        manager.writeBoolean(
                                SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE, prefs.googleLensAvailable);
                        manager.writeBoolean(
                                SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE, prefs.incognitoAvailable);
                    }

                    for (Consumer<SearchActivityPreferences> observer : self.mObservers) {
                        observer.accept(prefs);
                    }
                });
    }

    /**
     * Add a new preference change observer. This method guarantees that the newly added observer
     * will instantly receive information about current preferences.
     *
     * @param observer The observer to be added.
     */
    public static void addObserver(@NonNull Consumer<SearchActivityPreferences> observer) {
        ThreadUtils.assertOnUiThread();
        SearchActivityPreferencesManager self = get();
        if (!self.mObservers.hasObserver(observer)) {
            self.mObservers.addObserver(observer);
            observer.accept(self.mCurrentlyLoadedPreferences);
        }
    }

    /**
     * Creates the observer that will monitor for search engine changes. The native library and the
     * browser process must have been fully loaded before calling this.
     */
    public static void onNativeLibraryReady() {
        SearchActivityPreferencesManager self = get();
        TemplateUrlService service =
                TemplateUrlServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile());
        service.registerLoadListener(self);
        service.addObserver(self);
        if (!service.isLoaded()) {
            service.load();
        }
    }

    /**
     * Update feature availability. Retrieves availability information from multiple sources and
     * updates local cache.
     *
     * @param context Current context.
     * @param permissionDelegate The delegate serving permission information.
     */
    public static void updateFeatureAvailability(
            Context context, AndroidPermissionDelegate permissionDelegate) {
        SearchActivityPreferences prefs = getCurrent();
        setCurrentlyLoadedPreferences(
                new SearchActivityPreferences(
                        prefs.searchEngineName,
                        prefs.searchEngineUrl,
                        VoiceRecognitionUtil.isVoiceSearchEnabled(permissionDelegate),
                        LensController.getInstance()
                                .isLensEnabled(
                                        new LensQueryParams.Builder(
                                                        LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
                                                        false,
                                                        DeviceFormFactor
                                                                .isNonMultiDisplayContextOnTablet(
                                                                        context))
                                                .build()),
                        IncognitoUtils.isIncognitoModeEnabled(
                                ProfileManager.getLastUsedRegularProfile())),
                true);
    }

    /**
     * Retrieve the current search engine name and URL and update cached preferences. Requires that
     * the Native libraries are initialized.
     */
    private void updateDefaultSearchEngineInfo() {
        // Getting an instance of the TemplateUrlService requires that the native library be
        // loaded, but the TemplateUrlService also itself needs to be initialized.
        TemplateUrlService service =
                TemplateUrlServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile());

        // Update the URL that we show for zero-suggest.
        TemplateUrl dseTemplateUrl = service.getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl == null) return;

        GURL url = new GURL(service.getSearchEngineUrlFromTemplateUrl(dseTemplateUrl.getKeyword()));

        setCurrentlyLoadedPreferences(
                new SearchActivityPreferences(
                        dseTemplateUrl.getShortName(),
                        url.getOrigin(),
                        mCurrentlyLoadedPreferences.voiceSearchAvailable,
                        mCurrentlyLoadedPreferences.googleLensAvailable,
                        mCurrentlyLoadedPreferences.incognitoAvailable),
                true);
    }

    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.getForProfile(ProfileManager.getLastUsedRegularProfile())
                .unregisterLoadListener(this);
        updateDefaultSearchEngineInfo();
    }

    @Override
    public void onTemplateURLServiceChanged() {
        updateDefaultSearchEngineInfo();
    }

    /**
     * Reset the global instance of the SearchActivityPreferencesManager for the purpose of testing.
     */
    static void resetForTesting() {
        sInstance = null;
    }
}
