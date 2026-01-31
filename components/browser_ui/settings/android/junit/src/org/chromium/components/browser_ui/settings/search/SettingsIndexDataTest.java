// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/**
 * Unit tests for {@link SettingsIndexData}.
 *
 * <p>These tests validate the behavior of the core data model for settings search, including
 * adding/removing entries, text normalization, searching, and scoring logic.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsIndexDataTest {
    private static final String ROOT_FRAGMENT = "RootFragment";
    private static final String ID1 = id("id1");
    private static final String ID2 = id("id2");
    private SettingsIndexData mIndexData;

    @Before
    public void setUp() {
        mIndexData = new SettingsIndexData();
    }

    private void addEntry(String header, String key, String title, String summary, String frag) {
        String id = PreferenceParser.createUniqueId("class", key);
        mIndexData.addEntry(
                id,
                new SettingsIndexData.Entry.Builder(id, key, title, frag)
                        .setHeader(header)
                        .setSummary(summary)
                        .setFragment(frag)
                        .build());
    }

    private static String id(String key) {
        return PreferenceParser.createUniqueId("class", key);
    }

    private static String id(String parentFragment, String key) {
        return PreferenceParser.createUniqueId(parentFragment, key);
    }

    /** Tests the basic functionality of adding and retrieving an entry. */
    @Test
    public void testAddAndGetEntry() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title 1", "Parent1")
                        .setHeader("Header 1")
                        .setSummary("Summary 1")
                        .build();

        mIndexData.addEntry(ID1, entry);

        assertEquals("Should retrieve the correct entry.", entry, mIndexData.getEntry(ID1));
        assertNull("Should return null for a non-existent key.", mIndexData.getEntry(ID2));
    }

    /** Tests that adding a duplicate key throws the expected exception. */
    @Test(expected = IllegalStateException.class)
    public void testAddEntry_throwsOnDuplicateKey() {
        mIndexData.addEntry(
                ID1,
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title 1", "P1")
                        .setHeader("Header 1")
                        .build());
        // This second call with the same key should throw.
        mIndexData.addEntry(
                ID1,
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title 2", "P2")
                        .setHeader("Header 2")
                        .build());
    }

    @Test
    public void testRemoveEntry() {
        SettingsIndexData.Entry entry =
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title 1", "Parent1")
                        .setHeader("Header 1")
                        .setFragment("FragmentToKeep")
                        .build();
        mIndexData.addEntry(ID1, entry);

        mIndexData.removeEntry(ID1);

        assertNull("Entry should be removed.", mIndexData.getEntry(ID1));
    }

    @Test
    public void testFinalizeIndex_prunesOrphans() {
        // Setup: A -> B -> C hierarchy.
        // A is the top-level preference on the root screen.
        mIndexData.addEntry(
                id("pref_A"),
                new SettingsIndexData.Entry.Builder(
                                id("pref_A"), "pref_A", "Title A", ROOT_FRAGMENT)
                        .setFragment("FragmentB")
                        .build());
        mIndexData.addEntry(
                id("pref_B"),
                new SettingsIndexData.Entry.Builder(id("pref_B"), "pref_B", "Title B", "FragmentB")
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                id("pref_C"),
                new SettingsIndexData.Entry.Builder(id("pref_C"), "pref_C", "Title C", "FragmentC")
                        .build());

        mIndexData.addChildParentLink("FragmentB", id("pref_A"));
        mIndexData.addChildParentLink("FragmentC", id("pref_B"));

        mIndexData.removeEntry(id("pref_A"));

        mIndexData.resolveIndex(ROOT_FRAGMENT);

        // Assertions:
        assertNull("Parent link pref_A should be gone.", mIndexData.getEntry(id("pref_A")));
        assertNull(
                "Orphaned child pref_B should have been pruned.",
                mIndexData.getEntry(id("pref_B")));
        assertNull(
                "Orphaned grandchild pref_C should have been pruned.",
                mIndexData.getEntry(id("pref_C")));
    }

    @Test
    public void testAddEntry_removeTags() {
        mIndexData.addEntry(
                id("key_summary"),
                new SettingsIndexData.Entry.Builder(id("key_summary"), "key_summary", "Other", "P1")
                        .setHeader("Header 1")
                        .setSummary("<link>Contains link text</link>")
                        .build());
        var entry = mIndexData.getEntry(id("key_summary"));
        assertEquals("Contains link text", entry.summary);
    }

    @Test
    public void testFinalizeIndex_handlesMultiParentCorrectly() {
        // Setup: A child fragment (FragmentC) is reachable from two different parents (A and B).
        String ida = id("FragmentA", "pref_A");
        String idb = id("FragmentB", "pref_B");
        String idc = id("FragmentC", "pref_C");

        mIndexData.addEntry(
                ida,
                new SettingsIndexData.Entry.Builder(ida, "pref_A", "Title A", ROOT_FRAGMENT)
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                idb,
                new SettingsIndexData.Entry.Builder(idb, "pref_B", "Title B", ROOT_FRAGMENT)
                        .setFragment("FragmentC")
                        .build());
        mIndexData.addEntry(
                idc,
                new SettingsIndexData.Entry.Builder(idc, "pref_C", "Title C", "FragmentC").build());

        mIndexData.addChildParentLink("FragmentC", ida);
        mIndexData.addChildParentLink("FragmentC", idb);

        mIndexData.removeEntry(ida);

        mIndexData.resolveIndex(ROOT_FRAGMENT);

        assertNull("Pruned parent pref_A should be gone.", mIndexData.getEntry(ida));
        assertNotNull(
                "The remaining parent link pref_B should still exist.", mIndexData.getEntry(idb));
        assertNotNull(
                "Child pref_C should NOT be pruned as it's still reachable.",
                mIndexData.getEntry(idc));
        assertEquals(
                "Child's header should be resolved via the remaining parent B.",
                "Title B",
                mIndexData.getEntry(idc).header);
    }

    /** Tests the core search functionality, including scoring and result ordering. */
    @Test
    public void testSearch_scoringAndOrder() {
        // Setup: Add entries designed to test different scoring levels.
        mIndexData.addEntry(
                id("key_summary"),
                new SettingsIndexData.Entry.Builder(id("key_summary"), "key_summary", "Other", "P1")
                        .setHeader("Header 1")
                        .setSummary("Contains the word privacy")
                        .build());
        mIndexData.addEntry(
                id("key_title_partial"),
                new SettingsIndexData.Entry.Builder(
                                id("key_title_partial"), "key_title_partial", "Privacy Guide", "P2")
                        .setHeader("Header 2")
                        .build());
        mIndexData.addEntry(
                id("key_title_exact"),
                new SettingsIndexData.Entry.Builder(
                                id("key_title_exact"), "key_title_exact", "Privacy", "P3")
                        .setHeader("Header 2")
                        .build());

        // Action: Perform the search.
        SettingsIndexData.SearchResults results = mIndexData.search("privacy");
        List<SettingsIndexData.Entry> items = results.getItems();

        // Assertions:
        assertEquals("Should find all three matching entries.", 3, items.size());
        // 1. The exact title match should have the highest score and be first.
        assertEquals(id("key_title_exact"), items.get(0).id);
        // 2. The partial title match should be second.
        assertEquals(id("key_title_partial"), items.get(1).id);
        // 3. The summary match should have the lowest score and be last.
        assertEquals(id("key_summary"), items.get(2).id);
    }

    @Test
    public void testGroupByHeader() {
        addEntry("header1", "item12", "TitleItem12", "SummaryItem12", "P12");
        addEntry("header2", "item21", "TitleItem21", "SummaryItem21", "P21");
        addEntry("header3", "item31", "TitleItem31", "SummaryItem31", "P31");
        addEntry("header2", "item22", "TitleItem22", "SummaryItem22", "P22");
        addEntry("header1", "item11", "TitleItem11", "SummaryItem11", "P11");

        SettingsIndexData.SearchResults results = mIndexData.search("Item");
        List<SettingsIndexData.Entry> items = results.groupByHeader();
        assertEquals("header1", items.get(0).header);
        assertEquals("header1", items.get(1).header);
        assertEquals("header2", items.get(2).header);
        assertEquals("header2", items.get(3).header);
        assertEquals("header3", items.get(4).header);
    }

    /** Tests that the text normalization (diacritic stripping) works correctly. */
    @Test
    public void testSearch_normalization_stripsDiacritics() {
        // Setup: Add an entry with an accented character.
        mIndexData.addEntry(
                id("key_resume"),
                new SettingsIndexData.Entry.Builder(
                                id("key_resume"), "key_resume", "Resumé Settings", "P1")
                        .setHeader("Header 1")
                        .build());

        // Action: Search using the un-accented version of the word.
        SettingsIndexData.SearchResults results = mIndexData.search("resume");

        // Assertion: The search should find the correct entry.
        assertEquals("Should find one match.", 1, results.getItems().size());
        assertEquals(id("key_resume"), results.getItems().get(0).id);
    }

    /** Tests that an empty or non-matching search returns no results. */
    @Test
    public void testSearch_noMatches() {
        mIndexData.addEntry(
                ID1,
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title", "P1")
                        .setHeader("Header")
                        .setSummary("Summary")
                        .build());

        assertTrue(
                "Searching for a non-existent term should return empty results.",
                mIndexData.search("nonexistent").isEmpty());
        assertTrue(
                "Searching for an empty string should return empty results.",
                mIndexData.search("").isEmpty());
    }

    @Test
    public void testClear_removesAllEntriesAndRelationships() {
        mIndexData.addEntry(
                ID1,
                new SettingsIndexData.Entry.Builder(ID1, "key1", "Title 1", "ParentFragment")
                        .build());
        mIndexData.addChildParentLink("ChildFragment", ID1);

        assertFalse(
                "Entries map should not be empty before clear.",
                mIndexData.getEntriesForTesting().isEmpty());
        assertFalse(
                "Parent-child map should not be empty before clear.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());

        mIndexData.clear();

        assertTrue(
                "Entries map should be empty after clear.",
                mIndexData.getEntriesForTesting().isEmpty());
        assertTrue(
                "Parent-child map should be empty after clear.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());
    }

    @Test
    public void testSearch_normalization_stripsNonWesternDiacritics() {
        // 1. Arabic Example: "Marhaban"
        // Title has Tashkeel (vowel marks): "مَرْحَبًا"
        // Search query has no marks: "مرحبا"
        mIndexData.addEntry(
                id("key_arabic"),
                new SettingsIndexData.Entry.Builder(
                                id("key_arabic"), "key_arabic", "مَرْحَبًا", "P1")
                        .setHeader("Header 1")
                        .build());

        // 2. Hebrew Example: "Shalom"
        // Title has Nikkud (dots): "שָׁלוֹם"
        // Search query has no dots: "שלום"
        mIndexData.addEntry(
                id("key_hebrew"),
                new SettingsIndexData.Entry.Builder(id("key_hebrew"), "key_hebrew", "שָׁלוֹם", "P1")
                        .setHeader("Header 1")
                        .build());

        SettingsIndexData.SearchResults arabicResults = mIndexData.search("مرحبا");
        assertEquals(
                "Should find Arabic match ignoring Tashkeel.", 1, arabicResults.getItems().size());
        assertEquals(id("key_arabic"), arabicResults.getItems().get(0).id);

        SettingsIndexData.SearchResults hebrewResults = mIndexData.search("שלום");
        assertEquals(
                "Should find Hebrew match ignoring Nikkud.", 1, hebrewResults.getItems().size());
        assertEquals(id("key_hebrew"), hebrewResults.getItems().get(0).id);
    }
}
