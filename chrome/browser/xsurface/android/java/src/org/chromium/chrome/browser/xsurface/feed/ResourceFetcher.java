// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import androidx.annotation.Nullable;

import java.util.List;

/**
 * Implemented in Chromium.
 *
 * Interface to provide network fetching.
 */
public interface ResourceFetcher {
    /**
     * Represents the key portion of an http header field. Header keys should be compared
     * case-insensitively.
     */
    public class Header {
        public String name;
        public String value;

        public Header(String name, String value) {
            this.name = name;
            this.value = value;
        }
    }

    /** Data structure to encapsulate the fetch request. */
    public class Request {
        /** Uri of the resource to be fetched. */
        public String uri;

        /** Http method used to fetch the resource. */
        public String method;

        /** List of headers for this request. */
        public List<Header> headers;

        /** Post data that needs to be sent along with the POST request. */
        public @Nullable byte[] postData;
    }

    /** Data structure to encapsulate the fetch response. */
    public interface Response {
        /** Whether the request was successful. */
        public boolean getSuccess();

        /** HTTP status code. */
        public int getStatusCode();

        /** List of headers for this response. */
        public List<Header> getHeaders();

        /** Raw data received. */
        public byte[] getRawData();
    }

    /** Notify that a request has responded. */
    public interface ResponseCallback {
        public void onResponse(Response response);
    }

    /**
     * Fetches a given url resource.
     * Can be called from any thread.
     */
    public void fetch(Request request, ResponseCallback responseCallback);
}
