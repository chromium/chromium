// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_3_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_INVALID;

import android.util.ArraySet;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link CachedZeroSuggestionsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CachedZeroSuggestionsManagerUnitTest {
    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        Assert.assertEquals(data1.getSuggestionsList(), data2.getSuggestionsList());
        Assert.assertEquals(data1.getGroupsInfo(), data2.getGroupsInfo());
    }

    /**
     * Build a dummy suggestions list.
     *
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<AutocompleteMatch> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            final int id = index + 1;
            list.add(
                    createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL)
                            .setPostContentType(hasPostData ? "Content Type " + id : null)
                            .setPostData(hasPostData ? new byte[] {4, 5, 6, (byte) id} : null)
                            .build());
        }

        return list;
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @return Newly constructed AutocompleteMatch.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(int id) {
        return createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL);
    }

    /**
     * Create and partially initialize suggestion builder constructing dummy OmniboxSuggestions.
     *
     * @param id Suggestion identifier used to initialize a unique suggestion content.
     * @param type Suggestion type.
     * @return Newly constructed AutocompleteMatch.
     */
    private AutocompleteMatchBuilder createSuggestionBuilder(
            int id, @OmniboxSuggestionType int type) {
        return AutocompleteMatchBuilder.searchWithType(type)
                .setDisplayText("dummy text " + id)
                .setDescription("dummy description " + id);
    }

    @Test
    @SmallTest
    public void setNewSuggestions_cachedSuggestionsWithPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                AutocompleteResult.fromCache(buildDummySuggestionsList(2, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void setNewSuggestions_cachedSuggestionsWithoutPostdataBeforeAndAfterAreSame() {
        AutocompleteResult dataToCache =
                AutocompleteResult.fromCache(buildDummySuggestionsList(2, false), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void setNewSuggestions_DoNotcacheClipboardSuggestions() {
        List<AutocompleteMatch> mix_list =
                Arrays.asList(
                        createSuggestionBuilder(1, OmniboxSuggestionType.CLIPBOARD_IMAGE).build(),
                        createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                        createSuggestionBuilder(3, OmniboxSuggestionType.CLIPBOARD_TEXT).build(),
                        createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());
        List<AutocompleteMatch> expected_list =
                Arrays.asList(
                        createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                        createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(mix_list, null);
        AutocompleteResult dataToExpected = AutocompleteResult.fromCache(expected_list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToExpected, dataFromCache);
    }

    @Test
    @SmallTest
    public void groupsDetails_restoreDetailsFromEmptyCache() {
        // Note: purge cache explicitly, because tests are run on an actual device
        // and cache may hold content from other test runs.
        AutocompleteResult dataToCache = AutocompleteResult.fromCache(null, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void groupsDetails_cacheAllSaneGroupConfig() {
        var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_3_WITH_HEADER)
                        .build();

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void groupsDetails_restoreInvalidGroupsDetailsFromCache() {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        var groupsDetails =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_1_NO_HEADER)
                        .build();

        // Write to disk.
        AutocompleteResult dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Check that it works the first time (group details are correct).
        // Only the INVALID_GROUP should be removed.
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);

        // Truncate the data.
        var data =
                manager.readString(
                        ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, null);
        data = data.substring(0, data.length() - 10);
        manager.writeString(ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, data);
        dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, AutocompleteResult.fromCache(null, null));
        Assert.assertNull(
                manager.readString(
                        ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, null));

        // Corrupt the data.
        manager.writeString(
                ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, "abcdefgh");
        dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, AutocompleteResult.fromCache(null, null));
        Assert.assertNull(
                manager.readString(
                        ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO, null));

        // Remove the data.
        manager.removeKey(ChromePreferenceKeys.OMNIBOX_CACHED_ZERO_SUGGEST_GROUPS_INFO);
        dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, AutocompleteResult.fromCache(null, null));
    }

    @Test
    @SmallTest
    public void dropSuggestions_suggestionsWithValidGroupsAssociation() {
        List<AutocompleteMatch> list = buildDummySuggestionsList(2, false);
        list.add(createSuggestionBuilder(33).setGroupId(1).build());

        var groupsDetails =
                GroupsInfo.newBuilder().putGroupConfigs(1, SECTION_2_WITH_HEADER).build();

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void dropSuggestions_suggestionsWithInvalidGroupsAssociation() {
        List<AutocompleteMatch> listExpected = buildDummySuggestionsList(2, false);
        List<AutocompleteMatch> listToCache = buildDummySuggestionsList(2, false);
        listToCache.add(createSuggestionBuilder(33).setGroupId(1).build());

        AutocompleteResult dataExpected = AutocompleteResult.fromCache(listExpected, null);
        AutocompleteResult dataToCache = AutocompleteResult.fromCache(listToCache, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataExpected, dataFromCache);
    }

    @Test
    @SmallTest
    public void malformedCache_dropsMissingSuggestions() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();

        // Save one valid suggestion to cache.
        AutocompleteResult dataToCache =
                AutocompleteResult.fromCache(buildDummySuggestionsList(1, true), null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Signal that there's actually 2 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_LIST_SIZE, 2);

        // Construct an expected raw suggestion list content. This constitutes one valid entry
        // and 1 totally empty entry.
        AutocompleteResult rawDataFromCache =
                AutocompleteResult.fromCache(buildDummySuggestionsList(1, true), null);
        rawDataFromCache.getSuggestionsList().add(new AutocompleteMatchBuilder().build());

        // readCachedSuggestionList makes full attempt to restore whatever could be scraped from the
        // cache.
        List<AutocompleteMatch> readList =
                CachedZeroSuggestionsManager.readCachedSuggestionList(manager);
        Assert.assertEquals(2, readList.size());
        assertAutocompleteResultEquals(
                AutocompleteResult.fromCache(readList, null), rawDataFromCache);

        // Cache recovery however should be smart here and remove items that make no sense.
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, dataToCache);
    }

    @Test
    @SmallTest
    public void removeInvalidSuggestions_dropsInvalidSuggestionsAndGroupsDetails() {
        // Write 3 wrong group groupsDetails to the cache
        var groupsDetailsExpected =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(12, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(AutocompleteMatch.INVALID_GROUP, SECTION_INVALID)
                        .build();
        var groupsDetailsWithInvalidItems =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(12, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(AutocompleteMatch.INVALID_GROUP, SECTION_INVALID)
                        .build();

        List<AutocompleteMatch> listExpected = buildDummySuggestionsList(2, false);
        listExpected.add(createSuggestionBuilder(72).setGroupId(12).build());

        List<AutocompleteMatch> listWithInvalidItems = buildDummySuggestionsList(2, false);
        listWithInvalidItems.add(createSuggestionBuilder(72).setGroupId(12).build());
        listWithInvalidItems.add(
                createSuggestionBuilder(73)
                        .setGroupId(12)
                        .setUrl(JUnitTestGURLs.INVALID_URL)
                        .build());
        listWithInvalidItems.add(createSuggestionBuilder(74).setGroupId(34).build());

        AutocompleteResult dataWithInvalidItems =
                AutocompleteResult.fromCache(listWithInvalidItems, groupsDetailsWithInvalidItems);
        AutocompleteResult dataExpected =
                AutocompleteResult.fromCache(listExpected, groupsDetailsExpected);

        CachedZeroSuggestionsManager.removeInvalidSuggestionsAndGroupsDetails(
                dataWithInvalidItems.getSuggestionsList(),
                dataWithInvalidItems.getGroupsInfo().getGroupConfigsMap());
        assertAutocompleteResultEquals(dataWithInvalidItems, dataExpected);
    }

    @Test
    @SmallTest
    public void cacheAndRestoreSuggestionSubtypes() {
        List<AutocompleteMatch> list =
                Arrays.asList(
                        createSuggestionBuilder(
                                        1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                                .addSubtype(1)
                                .addSubtype(4)
                                .build(),
                        createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL)
                                .addSubtype(17)
                                .build(),
                        createSuggestionBuilder(3, OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                                .addSubtype(2)
                                .addSubtype(10)
                                .addSubtype(30)
                                .build(),
                        createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void rejectCacheIfSubtypesAreMalformed() {
        List<AutocompleteMatch> list =
                Arrays.asList(
                        createSuggestionBuilder(
                                        1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                                .addSubtype(1)
                                .addSubtype(4)
                                .build(),
                        createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL)
                                .addSubtype(17)
                                .build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Insert garbage for the Suggestion Subtypes.
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("invalid");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(1),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(AutocompleteResult.fromCache(null, null), dataFromCache);
    }

    @Test
    @SmallTest
    public void rejectCacheIfSubtypesIncludeNull() {
        List<AutocompleteMatch> list =
                Arrays.asList(
                        createSuggestionBuilder(
                                        1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                                .addSubtype(1)
                                .build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("null");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(0),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(AutocompleteResult.fromCache(null, null), dataFromCache);
    }
}
