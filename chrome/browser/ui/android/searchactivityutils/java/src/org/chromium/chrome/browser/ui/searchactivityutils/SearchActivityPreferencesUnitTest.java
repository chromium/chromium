// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link SearchActivityPreferences}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityPreferencesUnitTest {
    private SearchActivityPreferences mPreferences;

    @Before
    public void setUp() {
        mPreferences =
                new SearchActivityPreferences.Builder()
                        .setAccountEmail("email@a.b")
                        .setSearchEngineName("test")
                        .setSearchEngineUrl(new GURL("https://test.url"))
                        .build();
    }

    @Test
    public void builder_defaultValues() {
        SearchActivityPreferences prefs = new SearchActivityPreferences.Builder().build();

        assertNull(prefs.accountEmail);
        assertNull(prefs.searchEngineName);
        assertEquals(GURL.emptyGURL(), prefs.searchEngineUrl);
        assertEquals(
                SearchActivityPreferences.DEFAULT_VOICE_SEARCH_AVAILABILITY,
                prefs.voiceSearchAvailable);
        assertEquals(
                SearchActivityPreferences.DEFAULT_GOOGLE_LENS_AVAILABILITY,
                prefs.googleLensAvailable);
        assertEquals(
                SearchActivityPreferences.DEFAULT_INCOGNITO_AVAILABILITY, prefs.incognitoAvailable);
    }

    @Test
    public void builderTest_toBuilderPreservesAllValues() {
        SearchActivityPreferences copy = mPreferences.toBuilder().build();
        assertEquals(mPreferences, copy);
        assertEquals("email@a.b", copy.accountEmail);
        assertEquals("test", copy.searchEngineName);
        assertEquals(new GURL("https://test.url"), copy.searchEngineUrl);
        assertEquals(
                SearchActivityPreferences.DEFAULT_VOICE_SEARCH_AVAILABILITY,
                copy.voiceSearchAvailable);
        assertEquals(
                SearchActivityPreferences.DEFAULT_GOOGLE_LENS_AVAILABILITY,
                copy.googleLensAvailable);
        assertEquals(
                SearchActivityPreferences.DEFAULT_INCOGNITO_AVAILABILITY, copy.incognitoAvailable);
    }

    @Test
    public void preferenceTest_equalWithSameContent() {
        SearchActivityPreferences sameContent = mPreferences.toBuilder().build();
        assertEquals(mPreferences, sameContent);
        assertEquals(mPreferences.hashCode(), sameContent.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentVoiceAvailability() {
        boolean voiceSearchAvailable = mPreferences.voiceSearchAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setVoiceSearchAvailable(!voiceSearchAvailable).build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentLensAvailability() {
        boolean googleLensAvailable = mPreferences.googleLensAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setGoogleLensAvailable(!googleLensAvailable).build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentIncognitoAvailability() {
        boolean incognitoAvailable = mPreferences.incognitoAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setIncognitoAvailable(!incognitoAvailable).build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineName() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setSearchEngineName("other").build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineUrl() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setSearchEngineUrl(new GURL("https://other.url")).build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentEmail() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setAccountEmail("other@a.b").build();
        assertNotEquals(mPreferences, modified);
        assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }
}
