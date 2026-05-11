// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import static org.junit.Assert.assertThrows;

import android.content.Context;
import android.os.Bundle;

import androidx.fragment.app.testing.FragmentScenario;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Set;

/** Unit tests for {@link SearchIndexValidator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchIndexValidatorTest {

    private Context mContext;

    /** Mockable fragment class for testing. */
    public static class MockFragment extends PreferenceFragmentCompat {
        public static SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER;

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getContext()));
        }
    }

    private MockFragment mFragment;
    private PreferenceScreen mScreen;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        // Launch the fragment using FragmentScenario to ensure the PreferenceManager is
        // initialized.
        FragmentScenario<MockFragment> scenario =
                FragmentScenario.launchInContainer(MockFragment.class, Bundle.EMPTY);
        scenario.onFragment(
                fragment -> {
                    mFragment = fragment;
                    mScreen = fragment.getPreferenceScreen();
                });
    }

    @Test
    public void testValidateSearchIndex_MatchingParity_Success() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName()) {
                    @Override
                    public void updateDynamicPreferences(
                            Context context, SettingsIndexData indexData) {
                        indexData.addEntryForKey(
                                MockFragment.class.getName(), "test_key", "Title", null);
                    }
                };

        Preference pref = new Preference(mContext);
        pref.setKey("test_key");
        mScreen.addPreference(pref);

        SearchIndexValidator.validateSearchIndex(mFragment);
        // Should not throw.
    }

    @Test
    public void testValidateSearchIndex_MissingInIndex_Throws() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName());

        Preference pref = new Preference(mContext);
        pref.setKey("unindexed_key");
        mScreen.addPreference(pref);

        assertThrows(
                AssertionError.class, () -> SearchIndexValidator.validateSearchIndex(mFragment));
    }

    @Test
    public void testValidateSearchIndex_ExtraInIndex_Throws() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName()) {
                    @Override
                    public void updateDynamicPreferences(
                            Context context, SettingsIndexData indexData) {
                        indexData.addEntryForKey(
                                MockFragment.class.getName(), "missing_from_ui", "Title", null);
                    }
                };

        assertThrows(
                AssertionError.class, () -> SearchIndexValidator.validateSearchIndex(mFragment));
    }

    @Test
    public void testValidateSearchIndex_RemovedFromIndex_Ignored() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName()) {
                    @Override
                    public void updateDynamicPreferences(
                            Context context, SettingsIndexData indexData) {
                        // Key is in XML (simulated) but removed dynamically.
                        indexData.removeEntryForKey(MockFragment.class.getName(), "removed_key");
                    }
                };

        Preference pref = new Preference(mContext);
        pref.setKey("removed_key");
        mScreen.addPreference(pref);

        // Should not throw because removedKeys are tracked and ignored by validator.
        SearchIndexValidator.validateSearchIndex(mFragment);
    }

    @Test
    public void testValidateSearchIndex_ExplicitlyIgnored_Ignored() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName()) {
                    @Override
                    public Set<String> getIgnoredKeys() {
                        return Set.of("ignored_key");
                    }
                };

        Preference pref = new Preference(mContext);
        pref.setKey("ignored_key");
        mScreen.addPreference(pref);

        SearchIndexValidator.validateSearchIndex(mFragment);
        // Should not throw.
    }

    @Test
    public void testValidateSearchIndex_RecursiveSearch_Success() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName()) {
                    @Override
                    public void updateDynamicPreferences(
                            Context context, SettingsIndexData indexData) {
                        indexData.addEntryForKey(
                                MockFragment.class.getName(), "nested_key", "Title", null);
                    }
                };

        PreferenceCategory category = new PreferenceCategory(mContext);
        mScreen.addPreference(category);

        Preference pref = new Preference(mContext);
        pref.setKey("nested_key");
        category.addPreference(pref);

        SearchIndexValidator.validateSearchIndex(mFragment);
        // Should not throw.
    }

    @Test
    public void testValidateSearchIndex_InvisiblePreference_Ignored() {
        MockFragment.SEARCH_INDEX_DATA_PROVIDER =
                new BaseSearchIndexProvider(MockFragment.class.getName());

        Preference pref = new Preference(mContext);
        pref.setKey("invisible_key");
        pref.setVisible(false);
        mScreen.addPreference(pref);

        SearchIndexValidator.validateSearchIndex(mFragment);
        // Should not throw because invisible prefs aren't indexed and thus aren't validated.
    }
}
