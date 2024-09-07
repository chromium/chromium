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

import android.text.TextUtils;
import android.util.Base64;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.omnibox.AnswerTypeProto.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Function;

/** CachedZeroSuggestionsManager manages caching and restoring zero suggestions. */
public class CachedZeroSuggestionsManager {
    /** Save the content of the CachedZeroSuggestionsManager to SharedPreferences cache. */
    public static void saveToCache(@NonNull AutocompleteResult resultToCache) {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        cacheSuggestionList(manager, resultToCache.getSuggestionsList());
        cacheGroupsDetails(manager, resultToCache.getGroupsInfo());
    }

    /**
     * Read previously stored AutocompleteResult from cache.
     *
     * @return AutocompleteResult populated with the content of the SharedPreferences cache.
     */
    static @NonNull AutocompleteResult readFromCache() {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        List<AutocompleteMatch> suggestions =
                CachedZeroSuggestionsManager.readCachedSuggestionList(manager);
        GroupsInfo groupsDetails = CachedZeroSuggestionsManager.readCachedGroupsDetails(manager);
        removeInvalidSuggestionsAndGroupsDetails(suggestions, groupsDetails.getGroupConfigsMap());
        return AutocompleteResult.fromCache(suggestions, groupsDetails);
    }

    /**
     * Cache suggestion list in shared preferences.
     *
     * @param prefs Shared preferences manager.
     */
    private static void cacheSuggestionList(
            SharedPreferencesManager prefs, List<AutocompleteMatch> suggestions) {
        int numCachableSuggestions = 0;

        // Write 0 here to avoid something wrong in the for loop, and the real size will be updated
        // after the for loop.
        prefs.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 0);
        for (int i = 0; i < suggestions.size(); i++) {
            AutocompleteMatch suggestion = suggestions.get(i);
            if (!shouldCacheSuggestion(suggestion)) continue;

            prefs.writeString(
                    KEY_ZERO_SUGGEST_URL_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getUrl().serialize());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getDisplayText());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getDescription());
            prefs.writeInt(
                    KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getType());
            prefs.writeStringSet(
                    KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(numCachableSuggestions),
                    convertSet(suggestion.getSubtypes(), v -> v.toString()));
            prefs.writeBoolean(
                    KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.isSearchSuggestion());
            prefs.writeBoolean(
                    KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.isDeletable());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getPostContentType());
            prefs.writeString(
                    KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getPostData() == null
                            ? null
                            : Base64.encodeToString(suggestion.getPostData(), Base64.DEFAULT));
            prefs.writeInt(
                    KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(numCachableSuggestions),
                    suggestion.getGroupId());
            numCachableSuggestions++;
        }
        prefs.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, numCachableSuggestions);
    }

    /**
     * Restore suggestion list from shared preferences.
     *
     * @param prefs Shared preferences manager.
     * @return List of Omnibox suggestions previously cached in shared preferences.
     */
    @NonNull
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static List<AutocompleteMatch> readCachedSuggestionList(SharedPreferencesManager prefs) {
        int size = prefs.readInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, -1);
        if (size <= 1) {
            // Ignore case where we only have a single item on the list - it's likely
            // 'what-you-typed' suggestion.
            size = 0;
        }

        List<AutocompleteMatch> suggestions = new ArrayList<>(size);
        List<AutocompleteMatch.MatchClassification> classifications = new ArrayList<>();
        classifications.add(
                new AutocompleteMatch.MatchClassification(0, MatchClassificationStyle.NONE));
        for (int i = 0; i < size; i++) {
            // TODO(tedchoc): Answers in suggest were previously cached, but that could lead to
            //                stale or misleading answers for cases like weather.  Ignore any
            //                previously cached answers for several releases while any previous
            //                results are cycled through.
            String answerText =
                    prefs.readString(KEY_ZERO_SUGGEST_ANSWER_TEXT_PREFIX.createKey(i), null);
            if (!TextUtils.isEmpty(answerText)) continue;

            GURL url =
                    GURL.deserialize(
                            prefs.readString(KEY_ZERO_SUGGEST_URL_PREFIX.createKey(i), null));
            String displayText =
                    prefs.readString(KEY_ZERO_SUGGEST_DISPLAY_TEXT_PREFIX.createKey(i), null);
            String description =
                    prefs.readString(KEY_ZERO_SUGGEST_DESCRIPTION_PREFIX.createKey(i), null);
            int nativeType =
                    prefs.readInt(
                            KEY_ZERO_SUGGEST_NATIVE_TYPE_PREFIX.createKey(i),
                            AutocompleteMatch.INVALID_TYPE);
            boolean isSearchType =
                    prefs.readBoolean(KEY_ZERO_SUGGEST_IS_SEARCH_TYPE_PREFIX.createKey(i), false);
            boolean isDeletable =
                    prefs.readBoolean(KEY_ZERO_SUGGEST_IS_DELETABLE_PREFIX.createKey(i), false);
            String postContentType =
                    prefs.readString(KEY_ZERO_SUGGEST_POST_CONTENT_TYPE_PREFIX.createKey(i), null);
            String postDataStr =
                    prefs.readString(KEY_ZERO_SUGGEST_POST_CONTENT_DATA_PREFIX.createKey(i), null);
            byte[] postData =
                    postDataStr == null ? null : Base64.decode(postDataStr, Base64.DEFAULT);
            int groupId =
                    prefs.readInt(
                            KEY_ZERO_SUGGEST_GROUP_ID_PREFIX.createKey(i),
                            AutocompleteMatch.INVALID_GROUP);

            Set<Integer> subtypes = null;
            try {
                Set<String> subtypeStrings =
                        prefs.readStringSet(
                                KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(i), null);
                subtypes = convertSet(subtypeStrings, v -> Integer.parseInt(v));
            } catch (NumberFormatException e) {
                // Subtype information contains malformed elements, suggesting that the
                // entire cache may be damaged.
                return Collections.emptyList();
            }

            AutocompleteMatch suggestion =
                    new AutocompleteMatch(
                            nativeType,
                            subtypes,
                            isSearchType,
                            0,
                            0,
                            displayText,
                            classifications,
                            description,
                            classifications,
                            null,
                            null,
                            0,
                            null,
                            url,
                            GURL.emptyGURL(),
                            null,
                            isDeletable,
                            postContentType,
                            postData,
                            groupId,
                            null,
                            false,
                            null,
                            false,
                            null,
                            null);
            suggestions.add(suggestion);
        }

        return suggestions;
    }

    /**
     * Cache suggestion group details in shared preferences.
     *
     * @param prefs Shared preferences manager.
     * @param groupsDetails Map of Group ID to GroupConfig.
     */
    private static void cacheGroupsDetails(
            SharedPreferencesManager prefs, GroupsInfo groupsDetails) {
        prefs.writeString(
                ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO,
                Base64.encodeToString(groupsDetails.toByteArray(), Base64.DEFAULT));
    }

    /**
     * Restore group details from shared preferences.
     *
     * @param prefs Shared preferences manager.
     * @return Map of group ID to GroupConfig previously cached in shared preferences.
     */
    @NonNull
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static GroupsInfo readCachedGroupsDetails(SharedPreferencesManager prefs) {
        var encoded =
                prefs.readString(
                        ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, null);

        if (encoded != null) {
            try {
                var serialized = Base64.decode(encoded, Base64.DEFAULT);
                return GroupsInfo.parseFrom(serialized);
            } catch (IllegalArgumentException e) {
                // Bad Base64 encoding.
            } catch (InvalidProtocolBufferException e) {
                // Bad protobuf.
            }
            prefs.removeKey(ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO);
        }
        // Failed to decode or no cached groups info.
        return GroupsInfo.newBuilder().build();
    }

    /**
     * Remove all invalid entries for group details map and omnibox suggestions list.
     *
     * @param suggestions List of suggestions to scan for invalid entries.
     * @param groupsDetails Map of GroupConfig to scan for invalid entries.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void removeInvalidSuggestionsAndGroupsDetails(
            List<AutocompleteMatch> suggestions, Map<Integer, GroupConfig> groupsDetails) {
        // Remove all suggestions with no valid URL or pointing to nonexistent groups.
        for (int index = suggestions.size() - 1; index >= 0; index--) {
            final AutocompleteMatch suggestion = suggestions.get(index);
            final int groupId = suggestion.getGroupId();
            if (!suggestion.getUrl().isValid()
                    || suggestion.getUrl().isEmpty()
                    || (groupId != AutocompleteMatch.INVALID_GROUP
                            && !groupsDetails.containsKey(groupId))) {
                suggestions.remove(index);
            }
        }
    }

    /**
     * Check if the suggestion is needed to be cached.
     *
     * @param suggestion The AutocompleteMatch to check.
     * @return Whether or not the suggestion can be cached.
     */
    private static boolean shouldCacheSuggestion(AutocompleteMatch suggestion) {
        return suggestion.getAnswerType() == AnswerType.ANSWER_TYPE_UNSPECIFIED
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_URL
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_TEXT
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_IMAGE
                && suggestion.getType() != OmniboxSuggestionType.TILE_NAVSUGGEST;
    }

    /**
     * Convert the set of type T to set of type U objects.
     *
     * @param <T> Type of data held in the input set (inferred).
     * @param <U> Type of data held in the output set (inferred).
     * @param input Input set.
     * @param converter Function object that converts type T into type U.
     * @return A set of input objects converted to string.
     */
    private static <T, U> Set<U> convertSet(Set<T> input, Function<T, U> converter) {
        if (input == null) return null;

        Set<U> result = new ArraySet<>(input.size());
        for (T item : input) {
            result.add(converter.apply(item));
        }
        return result;
    }
}
