// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_3_WITH_HEADER;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link CachedZeroSuggestionsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedZeroSuggestionsManagerUnitTest {
    private static final int PAGE_CLASS = 0;

    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        assertEquals(data1.getSuggestionsList(), data2.getSuggestionsList());
        assertEquals(data1.getGroupsInfo(), data2.getGroupsInfo());
    }

    /**
     * Build a simple suggestions list.
     *
     * @param titlePrefix The prefix to use to populate suggestion data.
     * @param count How many suggestions to create.
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildSimpleSuggestionsList(String titlePrefix, int count) {
        List<AutocompleteMatch> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            list.add(
                    createSuggestionBuilder(
                                    titlePrefix, index + 1, OmniboxSuggestionType.HISTORY_URL)
                            .build());
        }

        return list;
    }

    /**
     * Create and partially initialize suggestion builder constructing simple OmniboxSuggestions.
     *
     * @param titlePrefix The prefix to use to populate suggestion data.
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @param type The type of suggestion to create.
     * @return AutocompleteMatchBuilder object to be used for further refinements.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(String titlePrefix, int id, int type) {
        return AutocompleteMatchBuilder.searchWithType(type)
                .setDisplayText(String.format("%s %d display text", titlePrefix, id))
                .setDisplayText(String.format("%s %d description", titlePrefix, id))
                .setUrl(new GURL("https://some.url/?q=" + titlePrefix));
    }

    @Test
    @SmallTest
    public void saveToCache_basicSaveRestoreTest() {
        var dataToCache = AutocompleteResult.fromCache(buildSimpleSuggestionsList("test", 2), null);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);
        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void saveToCache_dataIsAssociatedWithPageClass() {
        var dataToCache1 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test1", 2), null);
        var dataToCache2 =
                AutocompleteResult.fromCache(buildSimpleSuggestionsList("test2", 5), null);

        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache1);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 1, dataToCache2);

        var dataFromCache1 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        var dataFromCache2 = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS + 1);
        assertAutocompleteResultEquals(dataToCache1, dataFromCache1);
        assertAutocompleteResultEquals(dataToCache2, dataFromCache2);
    }

    @Test
    @SmallTest
    public void saveToCache_DoNotCacheClipboardSuggestions() {
        var mix_list =
                Arrays.asList(
                        createSuggestionBuilder("test", 1, OmniboxSuggestionType.CLIPBOARD_IMAGE)
                                .build(),
                        createSuggestionBuilder("test", 2, OmniboxSuggestionType.HISTORY_URL)
                                .build(),
                        createSuggestionBuilder("test", 3, OmniboxSuggestionType.CLIPBOARD_TEXT)
                                .build(),
                        createSuggestionBuilder("test", 4, OmniboxSuggestionType.SEARCH_HISTORY)
                                .build());
        var expected_list =
                Arrays.asList(
                        createSuggestionBuilder("test", 2, OmniboxSuggestionType.HISTORY_URL)
                                .build(),
                        createSuggestionBuilder("test", 4, OmniboxSuggestionType.SEARCH_HISTORY)
                                .build());

        var dataToCache = AutocompleteResult.fromCache(mix_list, null);
        var dataToExpected = AutocompleteResult.fromCache(expected_list, null);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToExpected, dataFromCache);
    }

    @Test
    @SmallTest
    public void eraseCachedData_removesAllCachedEntries() {
        var dataToCache = AutocompleteResult.fromCache(buildSimpleSuggestionsList("test", 3), null);

        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 2, dataToCache);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS + 4, dataToCache);

        var prefs = ContextUtils.getAppSharedPreferences();
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS)));
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 2)));
        assertTrue(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 4)));

        CachedZeroSuggestionsManager.eraseCachedData();

        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS)));
        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 2)));
        assertFalse(prefs.contains(CachedZeroSuggestionsManager.getCacheKey(PAGE_CLASS + 4)));
    }

    @Test
    @SmallTest
    public void readFromCache_restoreDetailsFromEmptyCache() {
        CachedZeroSuggestionsManager.eraseCachedData();

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(AutocompleteResult.fromCache(null, null), dataFromCache);
    }

    @Test
    @SmallTest
    public void readFromCache_restoreDetailsFromEmptyResult() {
        var dataToCache = AutocompleteResult.fromCache(null, null);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void saveToCache_preservesGroupsInfo() {
        var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_3_WITH_HEADER)
                        .build();

        var dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(PAGE_CLASS, dataToCache);

        var dataFromCache = CachedZeroSuggestionsManager.readFromCache(PAGE_CLASS);
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }
}
