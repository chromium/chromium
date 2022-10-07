// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.ArraySet;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link CachedZeroSuggestionsManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public class CachedZeroSuggestionsManagerUnitTest {
    /**
     * Compare two instances of CachedZeroSuggestionsManager to see if they are same, asserting if
     * they're not. Note that order is just as relevant as the content for caching.
     */
    void assertAutocompleteResultEquals(AutocompleteResult data1, AutocompleteResult data2) {
        final List<AutocompleteMatch> list1 = data1.getSuggestionsList();
        final List<AutocompleteMatch> list2 = data2.getSuggestionsList();
        Assert.assertEquals(list1, list2);

        final SparseArray<GroupConfig> groupsDetails1 = data1.getGroupsDetails();
        final SparseArray<GroupConfig> groupsDetails2 = data2.getGroupsDetails();
        Assert.assertEquals(groupsDetails1.size(), groupsDetails2.size());
        for (int index = 0; index < groupsDetails1.size(); index++) {
            Assert.assertEquals(groupsDetails1.keyAt(index), groupsDetails2.keyAt(index));
            Assert.assertEquals(groupsDetails1.valueAt(index), groupsDetails2.valueAt(index));
        }
    }

    /**
     * Build a dummy suggestions list.
     * @param count How many suggestions to create.
     * @param hasPostData If suggestions contain post data.
     *
     * @return List of suggestions.
     */
    private List<AutocompleteMatch> buildDummySuggestionsList(int count, boolean hasPostData) {
        List<AutocompleteMatch> list = new ArrayList<>();

        for (int index = 0; index < count; ++index) {
            final int id = index + 1;
            list.add(createSuggestionBuilder(id, OmniboxSuggestionType.HISTORY_URL)
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

    /**
     * Create a simple GroupConfig instance with supplied text and visibility.
     *
     * @param headerText The header text to apply to group config.
     * @param isHidden Whether the newly built group is default-collapsed.
     * @return Newly constructed GroupConfig.
     */
    private GroupConfig buildGroupConfig(String headerText, boolean isHidden) {
        return GroupConfig.newBuilder()
                .setHeaderText(headerText)
                .setVisibility(isHidden ? GroupConfig.Visibility.HIDDEN
                                        : GroupConfig.Visibility.DEFAULT_VISIBLE)
                .build();
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
        List<AutocompleteMatch> mix_list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.CLIPBOARD_IMAGE).build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
                createSuggestionBuilder(3, OmniboxSuggestionType.CLIPBOARD_TEXT).build(),
                createSuggestionBuilder(4, OmniboxSuggestionType.SEARCH_HISTORY).build());
        List<AutocompleteMatch> expected_list =
                Arrays.asList(createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL).build(),
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
        AutocompleteResult dataToCache = AutocompleteResult.EMPTY_RESULT;
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void groupsDetails_cacheAllSaneGroupConfig() {
        SparseArray<GroupConfig> groupsDetails = new SparseArray<>();
        groupsDetails.put(10, buildGroupConfig("Header For Group 10", false));
        groupsDetails.put(20, buildGroupConfig("Header For Group 20", false));
        groupsDetails.put(30, buildGroupConfig("Header For Group 30", false));
        AutocompleteResult dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataToCache, dataFromCache);
    }

    @Test
    @SmallTest
    public void groupsDetails_restoreInvalidGroupsDetailsFromCache() {
        SparseArray<GroupConfig> groupsDetails = new SparseArray<>();
        groupsDetails.put(
                AutocompleteMatch.INVALID_GROUP, buildGroupConfig("This group is invalid", true));
        groupsDetails.put(20, buildGroupConfig("Test", false));
        groupsDetails.put(30, buildGroupConfig("", false));

        // Write to disk.
        AutocompleteResult dataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Check that it works the first time (group details are correct).
        // Only the INVALID_GROUP should be removed.
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        Assert.assertEquals(2, dataFromCache.getGroupsDetails().size());

        // Partially remove data, rendering details invalid - check it no longer works.
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        manager.removeKey(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX.createKey(1));
        manager.removeKey(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_GROUP_TITLE_PREFIX.createKey(2));

        dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(dataFromCache, AutocompleteResult.EMPTY_RESULT);
    }

    @Test
    @SmallTest
    public void dropSuggestions_suggestionsWithValidGroupsAssociation() {
        List<AutocompleteMatch> list = buildDummySuggestionsList(2, false);
        list.add(createSuggestionBuilder(33).setGroupId(1).build());
        SparseArray<GroupConfig> groupsDetails = new SparseArray<>();
        groupsDetails.put(1, buildGroupConfig("Valid Header", true));

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

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

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
    public void malformedCache_dropsMissingGroupConfig() {
        // Clear cache explicitly, otherwise this test will be flaky until the suite is re-executed.
        ContextUtils.getAppSharedPreferences().edit().clear().apply();

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Write 3 wrong group groupsDetails to the cache
        SparseArray<GroupConfig> groupsDetails = new SparseArray<>();
        groupsDetails.put(12, buildGroupConfig("Valid group", true));
        groupsDetails.put(34, buildGroupConfig("", false));
        groupsDetails.put(AutocompleteMatch.INVALID_GROUP, buildGroupConfig("Invalid group", true));
        AutocompleteResult invalidDataToCache = AutocompleteResult.fromCache(null, groupsDetails);
        CachedZeroSuggestionsManager.saveToCache(invalidDataToCache);

        // Report that we actually have 4 items in the cache.
        manager.writeInt(ChromePreferenceKeys.KEY_ZERO_SUGGEST_HEADER_LIST_SIZE, 4);

        // Read raw suggestions from the cache. Note that the sparse array will only have 3 elements
        // because we put one item with INVALID_GROUP, and additional INVALID_GROUP will be deduced
        // from missing data with null title and default expanded state set to true.
        SparseArray<GroupConfig> rawGroupsDetails =
                CachedZeroSuggestionsManager.readCachedGroupsDetails(manager);
        Assert.assertEquals(3, rawGroupsDetails.size());
        Assert.assertEquals(rawGroupsDetails.get(12).getHeaderText(), "Valid group");
        Assert.assertEquals(
                rawGroupsDetails.get(12).getVisibility(), GroupConfig.Visibility.HIDDEN);
        Assert.assertEquals(rawGroupsDetails.get(34).getHeaderText(), "");
        Assert.assertEquals(
                rawGroupsDetails.get(34).getVisibility(), GroupConfig.Visibility.DEFAULT_VISIBLE);
        Assert.assertEquals(rawGroupsDetails.get(AutocompleteMatch.INVALID_GROUP).getHeaderText(),
                "Invalid group");
        Assert.assertEquals(rawGroupsDetails.get(AutocompleteMatch.INVALID_GROUP).getVisibility(),
                GroupConfig.Visibility.HIDDEN);

        // Cache recovery however should be smart here and remove items that make no sense.
        SparseArray<GroupConfig> wantGroupsDetails = new SparseArray<>();
        wantGroupsDetails.put(12, buildGroupConfig("Valid group", true));
        wantGroupsDetails.put(34, buildGroupConfig("", false));
        AutocompleteResult wantDataFromCache =
                AutocompleteResult.fromCache(null, wantGroupsDetails);
        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();

        assertAutocompleteResultEquals(dataFromCache, wantDataFromCache);
    }

    @Test
    @SmallTest
    public void removeInvalidSuggestions_dropsInvalidSuggestionsAndGroupsDetails() {
        // Write 3 wrong group groupsDetails to the cache
        SparseArray<GroupConfig> groupsDetailsExpected = new SparseArray<>();
        groupsDetailsExpected.put(12, buildGroupConfig("Valid group", true));

        SparseArray<GroupConfig> groupsDetailsWithInvalidItems = new SparseArray<>();
        groupsDetailsWithInvalidItems.put(12, buildGroupConfig("Valid group", true));
        groupsDetailsWithInvalidItems.put(
                AutocompleteMatch.INVALID_GROUP, buildGroupConfig("Invalid group", true));

        List<AutocompleteMatch> listExpected = buildDummySuggestionsList(2, false);
        listExpected.add(createSuggestionBuilder(72).setGroupId(12).build());

        List<AutocompleteMatch> listWithInvalidItems = buildDummySuggestionsList(2, false);
        listWithInvalidItems.add(createSuggestionBuilder(72).setGroupId(12).build());
        listWithInvalidItems.add(createSuggestionBuilder(73)
                                         .setGroupId(12)
                                         .setUrl(JUnitTestGURLs.getGURL(JUnitTestGURLs.INVALID_URL))
                                         .build());
        listWithInvalidItems.add(createSuggestionBuilder(74).setGroupId(34).build());

        AutocompleteResult dataWithInvalidItems =
                AutocompleteResult.fromCache(listWithInvalidItems, groupsDetailsWithInvalidItems);
        AutocompleteResult dataExpected =
                AutocompleteResult.fromCache(listExpected, groupsDetailsExpected);

        CachedZeroSuggestionsManager.removeInvalidSuggestionsAndGroupsDetails(
                dataWithInvalidItems.getSuggestionsList(), dataWithInvalidItems.getGroupsDetails());
        assertAutocompleteResultEquals(dataWithInvalidItems, dataExpected);
    }

    @Test
    @SmallTest
    public void cacheAndRestoreSuggestionSubtypes() {
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
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
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                        .addSubtype(1)
                        .addSubtype(4)
                        .build(),
                createSuggestionBuilder(2, OmniboxSuggestionType.HISTORY_URL)
                        .addSubtype(17)
                        .build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        // Insert garbage for the Suggestion Subtypes.
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("invalid");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(1),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(AutocompleteResult.EMPTY_RESULT, dataFromCache);
    }

    @Test
    @SmallTest
    public void rejectCacheIfSubtypesIncludeNull() {
        List<AutocompleteMatch> list = Arrays.asList(
                createSuggestionBuilder(1, OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                        .addSubtype(1)
                        .build());

        AutocompleteResult dataToCache = AutocompleteResult.fromCache(list, null);
        CachedZeroSuggestionsManager.saveToCache(dataToCache);

        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();
        final Set<String> garbageSubtypes = new ArraySet<>();
        garbageSubtypes.add("null");
        manager.writeStringSet(
                ChromePreferenceKeys.KEY_ZERO_SUGGEST_NATIVE_SUBTYPES_PREFIX.createKey(0),
                garbageSubtypes);

        AutocompleteResult dataFromCache = CachedZeroSuggestionsManager.readFromCache();
        assertAutocompleteResultEquals(AutocompleteResult.EMPTY_RESULT, dataFromCache);
    }
}
