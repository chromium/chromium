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

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteProto.AutocompleteResultProto;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.url.GURL;

import java.util.Locale;
import java.util.Set;

/** CachedZeroSuggestionsManager manages caching and restoring zero suggestions. */
public class CachedZeroSuggestionsManager {
    /** Jump-Start Omnibox: the context of the most recently visited page. */
    public static class JumpStartContext {
        /** The GURL representing the most recently visited page. */
        public final GURL url;

        /** {@link PageClassification} value associated with the most recently visited page. */
        public final int pageClass;

        public JumpStartContext(GURL url, int pageClass) {
            this.url = url;
            this.pageClass = pageClass;
        }
    }

    /** Persisted Search Engine metadata. */
    public static class SearchEngineMetadata {
        /** The keyword associated with the search engine. */
        public final String keyword;

        public SearchEngineMetadata(String keyword) {
            this.keyword = keyword;
        }
    }

    @VisibleForTesting
    /* package */ static final String KEY_JUMP_START_URL = "omnibox:jump_start:url";

    @VisibleForTesting
    /* package */ static final String KEY_JUMP_START_PAGE_CLASS = "omnibox:jump_start:page_class";

    @VisibleForTesting /* package */ static final String KEY_DSE_KEYWORD = "omnibox:dse:keyword";

    @VisibleForTesting
    /* package */ static final Set<String> ADDITIONAL_KEYS_TO_ERASE =
            Set.of(KEY_JUMP_START_URL, KEY_JUMP_START_PAGE_CLASS);

    /** Save the content of the CachedZeroSuggestionsManager to SharedPreferences cache. */
    @SuppressWarnings("ApplySharedPref")
    public static void saveToCache(int pageClass, @NonNull AutocompleteResult resultToCache) {
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
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putString(KEY_DSE_KEYWORD, metadata.keyword).apply();
    }

    /** Returns the details of the currently persisted Search Engine. */
    public static @Nullable SearchEngineMetadata readSearchEngineMetadata() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        var keyword = prefs.getString(KEY_DSE_KEYWORD, null);
        if (TextUtils.isEmpty(keyword)) return null;

        return new SearchEngineMetadata(keyword);
    }

    /**
     * Read previously stored AutocompleteResult from cache.
     *
     * @return AutocompleteResult populated with the content of the SharedPreferences cache.
     */
    static @NonNull AutocompleteResult readFromCache(int pageClass) {
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
    static void eraseCachedSuggestionsByPageClass(int pageClass) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String key = getCacheKey(pageClass);
        prefs.edit().remove(key).apply();
    }

    /** Save the context of the most recently visited page. */
    @SuppressWarnings("ApplySharedPref")
    public static void saveJumpStartContext(@Nullable JumpStartContext jsContext) {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        if (jsContext == null || GURL.isEmptyOrInvalid(jsContext.url)) {
            editor.remove(KEY_JUMP_START_URL);
            editor.remove(KEY_JUMP_START_PAGE_CLASS);
        } else {
            editor.putString(KEY_JUMP_START_URL, jsContext.url.getSpec());
            editor.putInt(KEY_JUMP_START_PAGE_CLASS, jsContext.pageClass);
        }
        // Note: this code has very little time to run. Be sure data is persisted. Don't use
        // asynchronous `apply()` method, because the asynchronously persisted details may never
        // make it to the data file.
        editor.commit();
    }

    /**
     * Read previously stored context of the most recently visited page.
     *
     * <p>This function always returns a valid object, even if there's no data to read, falling back
     * to the context of a NTP.
     */
    public static @NonNull JumpStartContext readJumpStartContext() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        String url = prefs.getString(KEY_JUMP_START_URL, UrlConstants.NTP_URL);
        int pageClass =
                prefs.getInt(
                        KEY_JUMP_START_PAGE_CLASS,
                        PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE);
        return new JumpStartContext(new GURL(url), pageClass);
    }

    /** Clean up data persisted by current Chrome versions. */
    public static void eraseCachedData() {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();

        for (var pageClass : PageClassification.values()) {
            editor.remove(getCacheKey(pageClass.getNumber()));
        }
        for (String key : ADDITIONAL_KEYS_TO_ERASE) {
            editor.remove(key);
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
    static String getCacheKey(int pageClass) {
        return String.format(Locale.getDefault(), "omnibox:cached_suggestions:%d", pageClass);
    }
}
