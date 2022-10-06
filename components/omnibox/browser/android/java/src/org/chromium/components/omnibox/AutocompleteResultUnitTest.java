// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.util.SparseArray;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.AutocompleteResult.GroupDetails;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for {@link AutocompleteResult}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public class AutocompleteResultUnitTest {
    private AutocompleteMatch buildSuggestionForIndex(int index) {
        return AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                .setDisplayText("Dummy Suggestion " + index)
                .setDescription("Dummy Description " + index)
                .build();
    }

    @Test
    public void autocompleteResult_sameContentsAreEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        // Element 0: 2 subtypes
        list1.get(0).getSubtypes().add(10);
        list1.get(0).getSubtypes().add(17);
        list2.get(0).getSubtypes().add(10);
        list2.get(0).getSubtypes().add(17);

        // Element 1: 0 subtypes.
        // Element 2: 1 subtype.
        list1.get(2).getSubtypes().add(4);
        list2.get(2).getSubtypes().add(4);

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", true));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertEquals(res1, res2);
        Assert.assertEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_itemsOutOfOrderAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(2), buildSuggestionForIndex(1), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", true));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_missingGroupsDetailsAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", true));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", true));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_groupsWithDifferentDefaultExpandedStateAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", true));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", false));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_extraGroupsDetailsAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(20, new GroupDetails("Test", false));
        groupsDetails2.put(30, new GroupDetails("Yikes", false));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, groupsDetails2);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_differentItemsAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(4));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list1, null);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list2, null);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1.hashCode(), res2.hashCode());
    }

    @Test
    public void autocompleteResult_differentGroupsDetailsAreNotEqual() {
        List<AutocompleteMatch> list = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(3));

        SparseArray<GroupDetails> groupsDetails1 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails2 = new SparseArray<>();
        SparseArray<GroupDetails> groupsDetails3 = new SparseArray<>();

        groupsDetails1.put(10, new GroupDetails("Hello", false));
        groupsDetails1.put(20, new GroupDetails("Test", false));

        groupsDetails2.put(10, new GroupDetails("Hello", false));
        groupsDetails2.put(15, new GroupDetails("Test", false));

        groupsDetails3.put(10, new GroupDetails("Hello", false));
        groupsDetails3.put(20, new GroupDetails("Test 2", false));

        AutocompleteResult res1 = AutocompleteResult.fromCache(list, groupsDetails1);
        AutocompleteResult res2 = AutocompleteResult.fromCache(list, groupsDetails2);
        AutocompleteResult res3 = AutocompleteResult.fromCache(list, groupsDetails3);

        Assert.assertNotEquals(res1, res2);
        Assert.assertNotEquals(res1, res3);
        Assert.assertNotEquals(res2, res3);
    }

    @Test
    public void autocompleteResult_differentSubtypesAreNotEqual() {
        List<AutocompleteMatch> list1 = Arrays.asList(
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(10)
                        .build(),
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(17)
                        .build());

        List<AutocompleteMatch> list2 = Arrays.asList(
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .addSubtype(10)
                        .build(),
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
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
        List<AutocompleteMatch> list2 = Arrays.asList(
                buildSuggestionForIndex(1), buildSuggestionForIndex(2), buildSuggestionForIndex(4));

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
        AutocompleteResult res2 = AutocompleteResult.EMPTY_RESULT;
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

        res = AutocompleteResult.fromCache(new ArrayList<>(), new SparseArray<>());
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

        res = AutocompleteResult.fromNative(
                0xfedcba98, new AutocompleteMatch[0], new int[0], new String[0], new boolean[0]);
        Assert.assertFalse(res.isFromCachedResult());
        res.notifyNativeDestroyed();
        Assert.assertFalse(res.isFromCachedResult());
    }
}
