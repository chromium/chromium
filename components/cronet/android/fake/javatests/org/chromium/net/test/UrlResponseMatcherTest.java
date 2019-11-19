// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test functionality of UrlResponseMatcher.
 */
@RunWith(AndroidJUnit4.class)
public class UrlResponseMatcherTest {
    @Test
    @SmallTest
    public void testCheckUrlNotNull() {
        try {
            UrlResponseMatcher matcher =
                    new UrlResponseMatcher(null, new FakeUrlResponse.Builder().build());
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testCheckResponseNotNull() {
        try {
            UrlResponseMatcher matcher = new UrlResponseMatcher("url", null);
            fail("Response not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Response is required.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testGetMatchingUrlResponse() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("TestBody".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);

        FakeUrlResponse found = matcher.getMatchingResponse(url, null, null, null);

        assertNotNull(found);
        assertEquals(found, response);
    }

    @Test
    @SmallTest
    public void testGetResponseWithBadUrlReturnsNull() {
        String url = "url";
        String urlWithoutResponse = "NO_RESPONSE";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("TestBody".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);

        FakeUrlResponse notFound =
                matcher.getMatchingResponse(urlWithoutResponse, null, null, null);

        assertNull(notFound);
    }
}
