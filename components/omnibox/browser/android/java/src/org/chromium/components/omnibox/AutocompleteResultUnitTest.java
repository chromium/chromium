// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_1_NO_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_2_WITH_HEADER;
import static org.chromium.components.omnibox.GroupConfigTestSupport.SECTION_3_WITH_HEADER;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.GroupsProto.GroupConfig;
import org.chromium.components.omnibox.GroupsProto.GroupsInfo;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link AutocompleteResult}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutocompleteResultUnitTest {
    private AutocompleteMatch buildSuggestionForIndex(int index) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Dummy Suggestion " + index)
                .setDescription("Dummy Description " + index)
                .build();
    }

    @Test
    public void autocompleteResult_sameContentsAreEqual() {
        List<AutocompleteMatch> list1 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));

        // Element 0: 2 subtypes
        list1.get(0).getSubtypes().add(10);
        list1.get(0).getSubtypes().add(17);
        list2.get(0).getSubtypes().add(10);
        list2.get(0).getSubtypes().add(17);

        // Element 1: 0 subtypes.
        // Element 2: 1 subtype.
        list1.get(2).getSubtypes().add(4);
        list2.get(2).getSubtypes().add(4);

        var groupsDetails1 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_3_WITH_HEADER)
                        .build();

        var groupsDetails2 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(
                                10, GroupConfig.newBuilder().mergeFrom(SECTION_1_NO_HEADER).build())
                        .putGroupConfigs(
                                20,
                                GroupConfig.newBuilder().mergeFrom(SECTION_2_WITH_HEADER).build())
                        .putGroupConfigs(
                                30,
                                GroupConfig.newBuilder().mergeFrom(SECTION_3_WITH_HEADER).build())
                        .build();

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_itemsOutOfOrderAreNotEqual() {
        var list1 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        var list2 =
                Arrays.asList(
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(3));

        var groupsDetails1 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .build();
        var groupsDetails2 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .build();

        var res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        var res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_missingGroupsDetailsAreNotEqual() {
        var list1 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        var list2 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));

        var groupsDetails1 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .build();
        var groupsDetails2 =
                GroupsInfo.newBuilder().putGroupConfigs(10, SECTION_1_NO_HEADER).build();

        var res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        var res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_extraGroupsDetailsAreNotEqual() {
        var list1 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        var list2 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));

        var groupsDetails1 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .build();
        var groupsDetails2 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .putGroupConfigs(30, SECTION_3_WITH_HEADER)
                        .build();

        var res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        var res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_differentItemsAreNotEqual() {
        List<AutocompleteMatch> list1 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(4));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_differentGroupsDetailsAreNotEqual() {
        var list =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));

        var groupsDetails1 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(20, SECTION_2_WITH_HEADER)
                        .build();
        var groupsDetails2 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(15, SECTION_2_WITH_HEADER)
                        .build();
        var groupsDetails3 =
                GroupsInfo.newBuilder()
                        .putGroupConfigs(10, SECTION_1_NO_HEADER)
                        .putGroupConfigs(
                                20,
                                GroupConfig.newBuilder()
                                        .mergeFrom(SECTION_2_WITH_HEADER)
                                        .setHeaderText("Woooo")
                                        .build())
                        .build();

        var res1 = AutocompleteResult.fromCache(list, groupsDetails1);
        var res2 = AutocompleteResult.fromCache(list, groupsDetails2);
        var res3 = AutocompleteResult.fromCache(list, groupsDetails3);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1, res3);
        Assert.assertNotEquals(res2, res3);
    }

    @Test
    public void autocompleteResult_differentSubtypesAreNotEqual() {
        List<AutocompleteMatch> list1 =
                Arrays.asList(
                        AutocompleteMatchBuilder.searchWithType(
                                        OmniboxSuggestionType.SEARCH_SUGGEST)
                                .addSubtype(10)
                                .build(),
                        AutocompleteMatchBuilder.searchWithType(
                                        OmniboxSuggestionType.SEARCH_SUGGEST)
                                .addSubtype(17)
                                .build());

        List<AutocompleteMatch> list2 =
                Arrays.asList(
                        AutocompleteMatchBuilder.searchWithType(
                                        OmniboxSuggestionType.SEARCH_SUGGEST)
                                .addSubtype(10)
                                .build(),
                        AutocompleteMatchBuilder.searchWithType(
                                        OmniboxSuggestionType.SEARCH_SUGGEST)
                                .addSubtype(4)
                                .build());

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);

        Assert.assertNotEquals(res1, res2);
    }

    @Test
    public void autocompleteResult_newItemsAreNotEqual() {
        List<AutocompleteMatch> list1 =
                Arrays.asList(buildSuggestionForIndex(1), buildSuggestionForIndex(2));
        List<AutocompleteMatch> list2 =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(4));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_emptyListsAreEqual() {
        final List<AutocompleteMatch> list1 = new ArrayList<>();
        final List<AutocompleteMatch> list2 = new ArrayList<>();
        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);
        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_nullAndEmptyListsAreEqual() {
        final List<AutocompleteMatch> list1 = new ArrayList<>();
        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(null, null);
        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_emptyAndNonEmptyListsAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(buildSuggestionForIndex(1));
        final List<AutocompleteMatch> list2 = new ArrayList<>();
        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);
        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void resultCreatedFromCacheIsIdentifiedAsCached() {
        AutocompleteResult res = new AutocompleteResult(0, null, null);
        Assert.assertTrue(res.isFromCachedResult());
        res.notifyNativeDestroyed();
        Assert.assertTrue(res.isFromCachedResult());

        res = AutocompleteResult.fromCache(new ArrayList<>(), GroupsInfo.newBuilder().build());
        Assert.assertTrue(res.isFromCachedResult());
        res.notifyNativeDestroyed();
        Assert.assertTrue(res.isFromCachedResult());
    }

    @Test
    public void resultCreatedFromNativeAreNotIdentifiedAsCached() {
        AutocompleteResult res = new AutocompleteResult(0x12345678, null, null);
        Assert.assertFalse(res.isFromCachedResult());
        res.notifyNativeDestroyed();
        Assert.assertFalse(res.isFromCachedResult());

        res = AutocompleteResult.fromNative(0xfedcba98, new AutocompleteMatch[0], new byte[0]);
        Assert.assertFalse(res.isFromCachedResult());
        res.notifyNativeDestroyed();
        Assert.assertFalse(res.isFromCachedResult());
    }

    @Test
    public void getDefaultMatch_emptyList() {
        AutocompleteResult emptyResult = new AutocompleteResult(0x12345678, null, null);
        Assert.assertNull(emptyResult.getDefaultMatch());
    }

    @Test
    public void getDefaultMatch_nonDefaultFirstMatch() {
        List<AutocompleteMatch> list =
                Arrays.asList(
                        buildSuggestionForIndex(1),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        AutocompleteResult autocompleteResult = new AutocompleteResult(0x12345678, list, null);
        Assert.assertNull(autocompleteResult.getDefaultMatch());
    }

    @Test
    public void getDefaultMatch_defaultFirstMatch() {
        List<AutocompleteMatch> list =
                Arrays.asList(
                        AutocompleteMatchBuilder.searchWithType(
                                        OmniboxSuggestionType.SEARCH_SUGGEST)
                                .setDisplayText("Dummy Suggestion 1")
                                .setDescription("Dummy Description 1")
                                .setAllowedToBeDefaultMatch(true)
                                .setInlineAutocompletion("inline_autocomplete")
                                .setAdditionalText("additional_text")
                                .build(),
                        buildSuggestionForIndex(2),
                        buildSuggestionForIndex(3));
        AutocompleteResult autocompleteResult = new AutocompleteResult(0x12345678, list, null);
        Assert.assertNotNull(autocompleteResult.getDefaultMatch());
        Assert.assertTrue(autocompleteResult.getDefaultMatch().allowedToBeDefaultMatch());
        Assert.assertEquals(
                "inline_autocomplete",
                autocompleteResult.getDefaultMatch().getInlineAutocompletion());
        Assert.assertEquals(
                "additional_text", autocompleteResult.getDefaultMatch().getAdditionalText());
    }
}
