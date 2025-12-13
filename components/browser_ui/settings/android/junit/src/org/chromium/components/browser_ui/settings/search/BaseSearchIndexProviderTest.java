// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.test.R;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link BaseSearchIndexProvider}.
 *
 * <p>These tests validate the core architectural behavior of the default search provider.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class BaseSearchIndexProviderTest {

    private Context mContext;
    private SettingsIndexData mIndexData;

    private static final String FRAGMENT_ROOT = "org.chromium.TestRootFragment";
    private static final String FRAGMENT_CHILD = "org.chromium.TestChildFragment";
    private static final String FRAGMENT_GRANDCHILD = "org.chromium.TestGrandchildFragment";

    private BaseSearchIndexProvider mRootProvider;
    private BaseSearchIndexProvider mChildProvider;
    private BaseSearchIndexProvider mGrandchildProvider;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mIndexData = new SettingsIndexData();

        mRootProvider = new BaseSearchIndexProvider(FRAGMENT_ROOT, R.xml.test_search_root_prefs);
        mChildProvider = new BaseSearchIndexProvider(FRAGMENT_CHILD, R.xml.test_search_child_prefs);
        mGrandchildProvider =
                new BaseSearchIndexProvider(
                        FRAGMENT_GRANDCHILD, R.xml.test_search_grandchild_prefs);
    }

    @Test
    public void testRegisterFragmentHeaders_buildsParentMapRecursively() {
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        providerMap.put(mRootProvider.getPrefFragmentName(), mRootProvider);
        providerMap.put(mChildProvider.getPrefFragmentName(), mChildProvider);
        providerMap.put(mGrandchildProvider.getPrefFragmentName(), mGrandchildProvider);
        Set<String> processedFragments = new HashSet<>();

        mRootProvider.registerFragmentHeaders(
                mContext, mIndexData, providerMap, processedFragments);

        Map<String, List<String>> parentMap = mIndexData.getChildFragmentToParentKeysForTesting();
        assertFalse("Parent map should not be empty after registration.", parentMap.isEmpty());

        // Verify Root -> Child link
        // "link_to_child" is defined in test_search_root_prefs.xml
        String expectedChildParentKey =
                PreferenceParser.createUniqueId(FRAGMENT_ROOT, "link_to_child");

        assertTrue(
                "Map should contain a link to the Child Fragment.",
                parentMap.containsKey(FRAGMENT_CHILD));
        assertEquals(
                "Child's parent preference should be the key from root XML.",
                expectedChildParentKey,
                parentMap.get(FRAGMENT_CHILD).get(0));

        // Verify Child -> Grandchild link
        // "link_to_grandchild" is defined in test_search_child_prefs.xml
        String expectedGrandchildParentKey =
                PreferenceParser.createUniqueId(FRAGMENT_CHILD, "link_to_grandchild");

        assertTrue(
                "Map should contain a link to the Grandchild Fragment.",
                parentMap.containsKey(FRAGMENT_GRANDCHILD));
        assertEquals(
                "Grandchild's parent preference should be the key from child XML.",
                expectedGrandchildParentKey,
                parentMap.get(FRAGMENT_GRANDCHILD).get(0));
    }

    @Test
    public void testInitPreferenceXml_addsEntriesFromXml() {
        mChildProvider.initPreferenceXml(mContext, mIndexData, new HashMap<>());

        Map<String, SettingsIndexData.Entry> entries = mIndexData.getEntriesForTesting();
        assertFalse("Index should not be empty after indexing.", entries.isEmpty());

        // "child_checkbox_pref" is defined in test_search_child_prefs.xml
        final String originalKey = "child_checkbox_pref";
        final String uniqueKey = PreferenceParser.createUniqueId(FRAGMENT_CHILD, originalKey);

        SettingsIndexData.Entry entry = entries.get(uniqueKey);
        assertNotNull("Preference should be indexed.", entry);
        assertEquals("Key should match.", originalKey, entry.key);
        assertEquals("Title should match the string resource.", "Child Setting Title", entry.title);
        assertEquals("Parent fragment name should be set.", FRAGMENT_CHILD, entry.parentFragment);
    }

    @Test
    public void testProviderWithNoXml_doesNothing() {
        BaseSearchIndexProvider providerWithNoXml =
                new BaseSearchIndexProvider("some.fragment.Name");
        Map<String, SearchIndexProvider> providerMap = new HashMap<>();
        Set<String> processedFragments = new HashSet<>();

        providerWithNoXml.initPreferenceXml(mContext, mIndexData, new HashMap<>());
        assertTrue(
                "Provider with no XML should not add any entries.",
                mIndexData.getEntriesForTesting().isEmpty());

        providerWithNoXml.registerFragmentHeaders(
                mContext, mIndexData, providerMap, processedFragments);
        assertTrue(
                "Provider with no XML should not add any parent-child links.",
                mIndexData.getChildFragmentToParentKeysForTesting().isEmpty());
    }
}
