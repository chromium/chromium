// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Implemented in Chromium.
 *
 * An object that can send an HTTP GET request and receive bytes in response. This interface should
 * only be used for fetching images.
 */
public interface ImageFetchClient {
    /** HTTP response. */
    public interface HttpResponse {
        /** HTTP status code if there was a response, or a net::Error if not. */
        default int status() {
            return -2; // net::FAILED
        }

        default byte[] body() {
            return new byte[0];
        }
    }

    /** HTTP response callback interface. */
    public interface HttpResponseConsumer {
        default void requestComplete(HttpResponse response) {}
    }

    /**
     * Send a GET request. Upon completion, asynchronously calls the consumer with all body bytes
     * from the response.
     *
     * @param url URL to request
     * @param responseConsumer The callback to call with the response
     * @return Request ID that can be passed to cancel()
     */
    default int sendCancelableRequest(String url, HttpResponseConsumer responseConsumer) {
        return 0;
    }

    /**
     * Send a GET request. TODO(iwells): Remove when the caller switches to the cancelable version.
     */
    default void sendRequest(String url, HttpResponseConsumer responseConsumer) {}

    /**
     * Cancel a pending request. Causes the request's response callback to be called with an empty
     * response body and net::Error::ERR_ABORTED.
     *
     * @param requestId ID of request to be canceled.
     */
    default void cancel(int requestId) {}
}
