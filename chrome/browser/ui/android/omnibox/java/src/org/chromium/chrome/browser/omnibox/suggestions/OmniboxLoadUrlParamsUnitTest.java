// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

/** Tests for {@link OmniboxLoadUrlParams}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxLoadUrlParamsUnitTest {
    private static final String TEST_URL = "http://www.example.org";

    @Test
    @SmallTest
    public void defaultValue() {
        OmniboxLoadUrlParams params =
                new OmniboxLoadUrlParams.Builder(TEST_URL, PageTransition.TYPED).build();

        assertEquals(params.url, TEST_URL);
        assertEquals(params.transitionType, PageTransition.TYPED);
        assertEquals(params.inputStartTimestamp, 0L);
        assertFalse(params.openInNewTab);
        assertNull(params.postDataType);
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
                        .setpostDataAndType(data, text)
                        .setAutocompleteLoadCallback(callback)
                        .build();

        assertEquals(params.url, TEST_URL);
        assertEquals(params.transitionType, PageTransition.TYPED);
        assertEquals(params.inputStartTimestamp, 100L);
        assertTrue(params.openInNewTab);
        assertEquals(params.postDataType, text);
        assertEquals(params.postData, data);
        assertEquals(params.callback, callback);
    }
}
