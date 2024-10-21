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
import android.util.Base64;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteProto.AutocompleteResultProto;
import org.chromium.components.omnibox.AutocompleteResult;

import java.util.Locale;

/** CachedZeroSuggestionsManager manages caching and restoring zero suggestions. */
public class CachedZeroSuggestionsManager {
    /** Save the content of the CachedZeroSuggestionsManager to SharedPreferences cache. */
    public static void saveToCache(int pageClass, @NonNull AutocompleteResult resultToCache) {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();

        var serializedBytes = resultToCache.serialize().toByteArray();
        prefs.edit()
                .putString(
                        getCacheKey(pageClass),
                        Base64.encodeToString(serializedBytes, Base64.DEFAULT))
                .apply();

        // This may take slightly more time once. Changes are applied asynchronously.
        eraseOldCachedData();
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
            prefs.edit().remove(key).apply();
        }
        return AutocompleteResult.fromCache(null, null);
    }

    /** Clean up data persisted by current Chrome versions. */
    public static void eraseCachedData() {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();

        for (var pageClass : PageClassification.values()) {
            editor.remove(getCacheKey(pageClass.getNumber()));
        }
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
        editor.apply();
    }

    @VisibleForTesting
    static String getCacheKey(int pageClass) {
        return String.format(Locale.getDefault(), "omnibox:cached_suggestions:%d", pageClass);
    }
}
