// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Test functionality of UrlResponseMatcher. */
@RunWith(AndroidJUnit4.class)
public class UrlResponseMatcherTest {
    @Test
    @SmallTest
    public void testCheckUrlNotNull() {
        NullPointerException e =
                assertThrows(
                        NullPointerException.class,
                        () -> new UrlResponseMatcher(null, new FakeUrlResponse.Builder().build()));
        assertThat(e).hasMessageThat().isEqualTo("URL is required.");
    }

    @Test
    @SmallTest
    public void testCheckResponseNotNull() {
        NullPointerException e =
                assertThrows(NullPointerException.class, () -> new UrlResponseMatcher("url", null));
        assertThat(e).hasMessageThat().isEqualTo("Response is required.");
    }

    @Test
    @SmallTest
    public void testGetMatchingUrlResponse() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("TestBody".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);

        FakeUrlResponse found = matcher.getMatchingResponse(url, null, null, null);

        assertThat(found).isEqualTo(response);
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

        assertThat(notFound).isNull();
    }
}
