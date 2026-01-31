// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.test.R;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link PreferenceParser}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PreferenceParserTest {

    private Context mContext;

    private static final String FRAGMENT_MAIN = "org.chromium.TestMainFragment";
    private static final String FRAGMENT_CHILD = "org.chromium.TestChildFragment";

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
    }

    @Test
    public void testParsePreferences_parsesBasicAttributesCorrectly() throws Exception {
        List<Bundle> parsedMetadata =
                PreferenceParser.parsePreferences(mContext, R.xml.test_search_root_prefs);
        assertNotNull("The parsed metadata should not be null.", parsedMetadata);

        // In test_search_root_prefs.xml, we have "link_to_child"
        @Nullable Bundle childBundle = findBundleByKey(parsedMetadata, "link_to_child");
        assertNotNull("The 'link_to_child' preference should be found.", childBundle);

        // Verify the basic attributes are correctly parsed.
        assertEquals("Go to Child", childBundle.getString(PreferenceParser.METADATA_TITLE));
        assertEquals(FRAGMENT_CHILD, childBundle.getString(PreferenceParser.METADATA_FRAGMENT));
    }

    @Test
    public void testParsePreferences_handlesPreferenceWithNoFragment() throws Exception {
        List<Bundle> parsedMetadata =
                PreferenceParser.parsePreferences(mContext, R.xml.test_search_root_prefs);

        // In test_search_root_prefs.xml, "root_item_1" has no android:fragment attribute.
        @Nullable Bundle simpleBundle = findBundleByKey(parsedMetadata, "root_item_1");
        assertNotNull("The 'root_item_1' preference should be found.", simpleBundle);

        assertNull(
                "The 'root_item_1' preference should not have a fragment attribute.",
                simpleBundle.getString(PreferenceParser.METADATA_FRAGMENT));
    }

    @Test
    public void testParseAndRegisterHeaders_addsParentLinks() {
        SettingsIndexData indexData = new SettingsIndexData();
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();

        // Simulate a provider for the child fragment
        providerMap.put(
                FRAGMENT_CHILD,
                new BaseSearchIndexProvider(FRAGMENT_CHILD, R.xml.test_search_child_prefs));

        Set<String> processedFragments = new HashSet<>();

        PreferenceParser.parseAndRegisterHeaders(
                mContext,
                R.xml.test_search_root_prefs,
                FRAGMENT_MAIN,
                indexData,
                providerMap,
                processedFragments);

        Map<String, List<String>> parentMap = indexData.getChildFragmentToParentKeysForTesting();
        assertFalse("The parent-child map should not be empty after parsing.", parentMap.isEmpty());

        assertTrue(
                "Map should contain an entry for the Child Fragment.",
                parentMap.containsKey(FRAGMENT_CHILD));

        List<String> childParents = parentMap.get(FRAGMENT_CHILD);
        assertEquals(
                "Child Fragment should have one parent in this context.", 1, childParents.size());

        String expectedUniqueId = PreferenceParser.createUniqueId(FRAGMENT_MAIN, "link_to_child");

        assertEquals(
                "The parent of the Child Fragment should be the 'link_to_child' preference.",
                expectedUniqueId,
                childParents.get(0));

        assertTrue(
                "The parsed root fragment should be marked as processed.",
                processedFragments.contains(FRAGMENT_MAIN));
    }

    @Test
    public void testParsePreferences_skipsTextMessagePreference() throws Exception {
        List<Bundle> parsedMetadata =
                PreferenceParser.parsePreferences(mContext, R.xml.test_search_root_prefs);

        Bundle textMessageBundle = findBundleByKey(parsedMetadata, "ignored_text_message");
        assertNull(
                "TextMessagePreference 'ignored_text_message' should be ignored/eliminated by the"
                        + " parser.",
                textMessageBundle);
    }

    @Nullable
    private Bundle findBundleByKey(List<Bundle> metadata, String key) {
        for (Bundle bundle : metadata) {
            if (key.equals(bundle.getString(PreferenceParser.METADATA_KEY))) {
                return bundle;
            }
        }
        return null;
    }
}
