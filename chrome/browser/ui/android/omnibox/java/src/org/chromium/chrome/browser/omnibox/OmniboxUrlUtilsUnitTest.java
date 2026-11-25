// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link OmniboxUrlUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class OmniboxUrlUtilsUnitTest {

    @Test
    public void isNtpUrl_nullUrlReturnsFalse() {
        assertFalse(OmniboxUrlUtils.isNtpUrl(null));
    }

    @Test
    public void isNtpUrl_emptyGurlReturnsTrue() {
        assertTrue(OmniboxUrlUtils.isNtpUrl(GURL.emptyGURL()));
    }

    @Test
    public void isNtpUrl_invalidGurlReturnsTrue() {
        assertTrue(OmniboxUrlUtils.isNtpUrl(new GURL("invalid-url")));
    }

    @Test
    public void isNtpUrl_standardNtpUrlReturnsTrue() {
        assertTrue(OmniboxUrlUtils.isNtpUrl(JUnitTestGURLs.NTP_URL));
        assertTrue(OmniboxUrlUtils.isNtpUrl(JUnitTestGURLs.NTP_NATIVE_URL));
    }

    @Test
    public void isNtpUrl_webUrlReturnsFalse() {
        assertFalse(OmniboxUrlUtils.isNtpUrl(JUnitTestGURLs.BLUE_1));
        assertFalse(OmniboxUrlUtils.isNtpUrl(JUnitTestGURLs.SEARCH_URL));
    }

    @Test
    public void isNtpUrl_otherInternalUrlReturnsFalse() {
        assertFalse(OmniboxUrlUtils.isNtpUrl(JUnitTestGURLs.ABOUT_BLANK));
        assertFalse(OmniboxUrlUtils.isNtpUrl(new GURL("chrome://settings")));
    }

    @Test
    public void getDirectVoiceNavigationUrl_nullMatchReturnsNull() {
        assertNull(OmniboxUrlUtils.getDirectVoiceNavigationUrl(null));
    }

    @Test
    public void getDirectVoiceNavigationUrl_searchSuggestionReturnsNull() {
        AutocompleteMatch match =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setUrl(JUnitTestGURLs.BLUE_1)
                        .build();
        assertNull(OmniboxUrlUtils.getDirectVoiceNavigationUrl(match));
    }

    @Test
    public void getDirectVoiceNavigationUrl_nonHttpOrInvalidUrlReturnsNull() {
        AutocompleteMatch chromeMatch =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setUrl(new GURL("chrome://settings"))
                        .build();
        assertNull(OmniboxUrlUtils.getDirectVoiceNavigationUrl(chromeMatch));

        AutocompleteMatch invalidMatch =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setUrl(new GURL("invalid-url"))
                        .build();
        assertNull(OmniboxUrlUtils.getDirectVoiceNavigationUrl(invalidMatch));
    }

    @Test
    public void getDirectVoiceNavigationUrl_validHttpOrHttpsUrlReturnsUrl() {
        AutocompleteMatch match =
                new AutocompleteMatchBuilder(OmniboxSuggestionType.URL_WHAT_YOU_TYPED)
                        .setUrl(JUnitTestGURLs.BLUE_1)
                        .build();
        assertEquals(JUnitTestGURLs.BLUE_1, OmniboxUrlUtils.getDirectVoiceNavigationUrl(match));
    }
}
