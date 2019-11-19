// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import org.chromium.net.UrlRequest;

import java.util.List;
import java.util.Map;

/**
 * A {@link ResponseMatcher} that matches {@link UrlRequest}s with a particular URL.
 */
public class UrlResponseMatcher implements ResponseMatcher {
    private final String mUrl;
    private final FakeUrlResponse mResponse;

    /**
     * Constructs a {@link UrlResponseMatcher} that responds to requests for URL {@code url} with
     * {@code response}.
     * @param url the URL that the response should be returned for
     * @param response the response to return if the URL matches the request's URL
     */
    public UrlResponseMatcher(String url, FakeUrlResponse response) {
        if (url == null) {
            throw new NullPointerException("URL is required.");
        }
        if (response == null) {
            throw new NullPointerException("Response is required.");
        }
        mUrl = url;
        mResponse = response;
    }

    @Override
    public FakeUrlResponse getMatchingResponse(
            String url, String httpMethod, List<Map.Entry<String, String>> headers, byte[] body) {
        return mUrl.equals(url) ? mResponse : null;
    }
}
