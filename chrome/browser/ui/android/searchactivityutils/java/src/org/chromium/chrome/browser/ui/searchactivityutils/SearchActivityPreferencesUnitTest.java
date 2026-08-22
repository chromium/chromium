// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import org.junit.Assert;
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

        Assert.assertNull(prefs.accountEmail);
        Assert.assertNull(prefs.searchEngineName);
        Assert.assertEquals(GURL.emptyGURL(), prefs.searchEngineUrl);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_VOICE_SEARCH_AVAILABILITY,
                prefs.voiceSearchAvailable);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_GOOGLE_LENS_AVAILABILITY,
                prefs.googleLensAvailable);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_INCOGNITO_AVAILABILITY, prefs.incognitoAvailable);
    }

    @Test
    public void builderTest_toBuilderPreservesAllValues() {
        SearchActivityPreferences copy = mPreferences.toBuilder().build();
        Assert.assertEquals(mPreferences, copy);
        Assert.assertEquals("email@a.b", copy.accountEmail);
        Assert.assertEquals("test", copy.searchEngineName);
        Assert.assertEquals(new GURL("https://test.url"), copy.searchEngineUrl);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_VOICE_SEARCH_AVAILABILITY,
                copy.voiceSearchAvailable);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_GOOGLE_LENS_AVAILABILITY,
                copy.googleLensAvailable);
        Assert.assertEquals(
                SearchActivityPreferences.DEFAULT_INCOGNITO_AVAILABILITY, copy.incognitoAvailable);
    }

    @Test
    public void preferenceTest_equalWithSameContent() {
        SearchActivityPreferences sameContent = mPreferences.toBuilder().build();
        Assert.assertEquals(mPreferences, sameContent);
        Assert.assertEquals(mPreferences.hashCode(), sameContent.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentVoiceAvailability() {
        boolean voiceSearchAvailable = mPreferences.voiceSearchAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setVoiceSearchAvailable(!voiceSearchAvailable).build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentLensAvailability() {
        boolean googleLensAvailable = mPreferences.googleLensAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setGoogleLensAvailable(!googleLensAvailable).build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentIncognitoAvailability() {
        boolean incognitoAvailable = mPreferences.incognitoAvailable;
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setIncognitoAvailable(!incognitoAvailable).build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineName() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setSearchEngineName("other").build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineUrl() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setSearchEngineUrl(new GURL("https://other.url")).build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentEmail() {
        SearchActivityPreferences modified =
                mPreferences.toBuilder().setAccountEmail("other@a.b").build();
        Assert.assertNotEquals(mPreferences, modified);
        Assert.assertNotEquals(mPreferences.hashCode(), modified.hashCode());
    }
}
