// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Utility class to validate that a PreferenceFragment's dynamically rendered UI matches its
 * SearchIndexProvider's indexing logic in debug builds.
 */
@NullMarked
public class SearchIndexValidator {

    /** Interface to allow caller-specific logic for populating the index data. */
    public interface SearchIndexDataBuilder {
        void buildIndexData(SearchIndexProvider provider, SettingsIndexData indexData);
    }

    /**
     * Validates that the dynamically rendered UI matches the search index. This throws an
     * AssertionError in Debug builds if a developer adds or removes a preference dynamically
     * without updating the corresponding SearchIndexProvider.
     *
     * @param fragment The fragment to validate.
     */
    public static void validateSearchIndex(PreferenceFragmentCompat fragment) {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        validateSearchIndex(
                fragment,
                (provider, indexData) -> {
                    // Passing an empty map since we only care about this fragment's keys, not its
                    // children.
                    provider.initPreferenceXml(fragment.getContext(), indexData, new HashMap<>());
                    provider.updateDynamicPreferences(fragment.getContext(), indexData);
                });
    }

    /**
     * Validates that the dynamically rendered UI matches the search index, using a custom builder.
     *
     * @param fragment The fragment to validate.
     * @param builder The logic to populate the SettingsIndexData.
     */
    public static void validateSearchIndex(
            PreferenceFragmentCompat fragment, SearchIndexDataBuilder builder) {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        SearchIndexProvider provider = null;
        try {
            provider =
                    (SearchIndexProvider)
                            fragment.getClass().getField("SEARCH_INDEX_DATA_PROVIDER").get(null);
        } catch (NoSuchFieldException | IllegalAccessException | ClassCastException e) {
            // Fragment doesn't declare a provider, or it's not accessible.
            return;
        }

        if (provider == null || !provider.isSearchable()) return;

        SettingsIndexData indexData = new SettingsIndexData();
        builder.buildIndexData(provider, indexData);

        Set<String> indexKeys = indexData.getKeys();
        Set<String> removedKeys = indexData.getRemovedKeys();
        Set<String> ignoredKeys = provider.getIgnoredKeys();

        Set<String> screenKeys = new HashSet<>();
        collectPreferenceKeys(fragment.getPreferenceScreen(), screenKeys);

        for (String screenKey : screenKeys) {
            String uniqueId =
                    PreferenceParser.createUniqueId(fragment.getClass().getName(), screenKey);
            if (!indexKeys.contains(screenKey)
                    && !ignoredKeys.contains(screenKey)
                    && !removedKeys.contains(uniqueId)) {
                throw new AssertionError(
                        "Preference '"
                                + screenKey
                                + "' was added dynamically to "
                                + fragment.getClass().getSimpleName()
                                + " but is missing from its "
                                + "SearchIndexProvider. Please update updateDynamicPreferences().");
            }
        }

        for (String indexKey : indexKeys) {
            if (!screenKeys.contains(indexKey) && !ignoredKeys.contains(indexKey)) {
                throw new AssertionError(
                        "Preference '"
                                + indexKey
                                + "' is listed in the SearchIndexProvider for "
                                + fragment.getClass().getSimpleName()
                                + " but is missing from the "
                                + "PreferenceScreen. Please update updateDynamicPreferences().");
            }
        }
    }

    private static void collectPreferenceKeys(@Nullable PreferenceGroup group, Set<String> keys) {
        if (group == null) return;
        for (int i = 0; i < group.getPreferenceCount(); i++) {
            Preference pref = group.getPreference(i);
            // Only visible preferences are considered searchable.
            if (pref.isVisible() && pref.getKey() != null) {
                keys.add(pref.getKey());
            }
            if (pref instanceof PreferenceGroup) {
                collectPreferenceKeys((PreferenceGroup) pref, keys);
            }
        }
    }
}
