// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_GROUP_ID_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.KEY_ZERO_SUGGEST_URL_PREFIX;

import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AutocompleteProto.AutocompleteResultProto;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.OmniboxFeatures;

import java.util.Locale;

/** CachedZeroSuggestionsManager manages caching and restoring zero suggestions. */
@NullMarked
public class CachedZeroSuggestionsManager {
    public interface OverridesForTesting {
        void saveToCache(@PageClassification int pageClass, AutocompleteResult r);

        AutocompleteResult readFromCache(@PageClassification int pageClass);
    }

    /** Persisted Search Engine metadata. */
    public static class SearchEngineMetadata {
        /** The keyword associated with the search engine. */
        public final String keyword;

        public SearchEngineMetadata(String keyword) {
            this.keyword = keyword;
        }
    }

    @VisibleForTesting /* package */ static final String KEY_DSE_KEYWORD = "omnibox:dse:keyword";

    private static @Nullable OverridesForTesting sOverridesForTesting;

    /** Save the content of the CachedZeroSuggestionsManager to SharedPreferences cache. */
    @SuppressWarnings("ApplySharedPref")
    public static void saveToCache(
            @PageClassification int pageClass, AutocompleteResult resultToCache) {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) return;
        if (sOverridesForTesting != null) {
            sOverridesForTesting.saveToCache(pageClass, resultToCache);
            return;
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();

        var serializedBytes = resultToCache.serialize().toByteArray();

        // Note: this code has very little time to run. Be sure data is persisted. Don't use
        // asynchronous `apply()` method, because the asynchronously persisted details may never
        // make it to the data file.
        prefs.edit()
                .putString(
                        getCacheKey(pageClass),
                        Base64.encodeToString(serializedBytes, Base64.DEFAULT))
                .commit();

        eraseOldCachedData();
    }

    /** Save the details related to currently selected Search Engine. */
    public static void saveSearchEngineMetadata(SearchEngineMetadata metadata) {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) return;
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putString(KEY_DSE_KEYWORD, metadata.keyword).apply();
    }

    /** Returns the details of the currently persisted Search Engine. */
    public static @Nullable SearchEngineMetadata readSearchEngineMetadata() {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) return null;
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        var keyword = prefs.getString(KEY_DSE_KEYWORD, null);
        if (TextUtils.isEmpty(keyword)) return null;

        return new SearchEngineMetadata(keyword);
    }

    /**
     * Read previously stored AutocompleteResult from cache.
     *
     * @param pageClass The page classification type for the cached suggestions.
     * @return AutocompleteResult populated with the content of the SharedPreferences cache.
     */
    static AutocompleteResult readFromCache(@PageClassification int pageClass) {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) {
            return AutocompleteResult.fromCache(null, null);
        }
        if (sOverridesForTesting != null) {
            return sOverridesForTesting.readFromCache(pageClass);
        }
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String key = getCacheKey(pageClass);

        var encoded = prefs.getString(key, null);
        if (encoded != null) {
            try {
                var deserialized =
                        AutocompleteResultProto.parseFrom(Base64.decode(encoded, Base64.DEFAULT));
                AutocompleteResult result = AutocompleteResult.deserialize(deserialized);
                return result;
            } catch (IllegalArgumentException e) {
                // Bad Base64 encoding.
            } catch (InvalidProtocolBufferException e) {
                // Bad protobuf.
            }
            eraseCachedSuggestionsByPageClass(pageClass);
        }
        return AutocompleteResult.fromCache(null, null);
    }

    /**
     * Erase previously stored AutocompleteResult for a given page class from cache.
     *
     * @param pageClass the PageClassification to clear cache for
     */
    static void eraseCachedSuggestionsByPageClass(@PageClassification int pageClass) {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) return;
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String key = getCacheKey(pageClass);
        prefs.edit().remove(key).apply();
    }

    /** Clean up data persisted by current Chrome versions. */
    public static void eraseCachedData() {
        if (!OmniboxFeatures.sServeJavaCachedZeroSuggest.isEnabled()) return;
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();

        for (@PageClassification int pageClass = PageClassification.MIN_VALUE;
                pageClass <= PageClassification.MAX_VALUE;
                pageClass++) {
            editor.remove(getCacheKey(pageClass));
        }
        // This is a best-effort cleanup which is okay if it doesn't complete before Chrome dies.
        editor.apply();

        eraseOldCachedData();
    }

    /** Clean up data persisted by older Chrome versions. */
    static void eraseOldCachedData() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();

        if (!prefs.contains(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE)) {
            return;
        }

        var editor = prefs.edit();
        editor.remove(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE);
        // In previously preserved context we cached up to 15 suggestions. Remove all old keys.
        for (int index = 0; index < 15; index++) {
            editor.remove(KEY_ZERO_SUGGEST_URL_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX.createKey(index));
            editor.remove(KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(index));
        }
        // This is a best-effort cleanup which is okay if it doesn't complete before Chrome dies.
        editor.apply();
    }

    @VisibleForTesting
    static String getCacheKey(@PageClassification int pageClass) {
        return String.format(Locale.getDefault(), "omnibox:cached_suggestions:%d", pageClass);
    }

    public static void setOverridesForTesting(OverridesForTesting value) {
        sOverridesForTesting = value;
        ResettersForTesting.register(() -> sOverridesForTesting = null);
    }
}
