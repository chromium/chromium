// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteDelegate.AutocompleteLoadCallback;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.HashMap;
import java.util.Map;

/** Tests for {@link OmniboxLoadUrlParams}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxLoadUrlParamsUnitTest {
    private static final String TEST_URL = "http://www.example.org";

    @Test
    @SmallTest
    public void defaultValue() {
        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build();

        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertEquals(0L, params.inputStartTimestamp);
        assertFalse(params.openInNewTab);
        assertTrue(params.extraHeaders.isEmpty());
        assertNull(params.postData);
        assertNull(params.callback);
    }

    @Test
    @SmallTest
    public void setAllValue() {
        String text = "text";
        byte[] data = new byte[] {0, 1, 2, 3, 4};
        AutocompleteLoadCallback callback =
                new AutocompleteLoadCallback() {
                    @Override
                    public void onLoadUrl(LoadUrlParams params, LoadUrlResult loadUrlResult) {}
                };
        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setInputStartTimestamp(100L)
                        .setOpenInNewTab(true)
                        .setPostData(data)
                        .setExtraHeaders(Map.of("Content-Type", text))
                        .setAutocompleteLoadCallback(callback)
                        .build();

        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertEquals(100L, params.inputStartTimestamp);
        assertTrue(params.openInNewTab);
        assertNotNull(params.extraHeaders);
        assertEquals(text, params.extraHeaders.get("Content-Type"));
        assertEquals(params.postData, data);
        assertEquals(params.callback, callback);
    }

    @Test
    @SmallTest
    public void setExtraHeaders() {
        Map<String, String> headers = new HashMap<>();
        headers.put("Authorization", "Bearer token123");
        headers.put("Custom-Header", "custom-value");

        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED)
                        .setExtraHeaders(headers)
                        .build();

        assertEquals(TEST_URL, params.url);
        assertEquals(PageTransition.TYPED, params.transitionType);
        assertNotNull(params.extraHeaders);
        assertEquals("Bearer token123", params.extraHeaders.get("Authorization"));
        assertEquals("custom-value", params.extraHeaders.get("Custom-Header"));
        assertEquals(2, params.extraHeaders.size());
    }
}
