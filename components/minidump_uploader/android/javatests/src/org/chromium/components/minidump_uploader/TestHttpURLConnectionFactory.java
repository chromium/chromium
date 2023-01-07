// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * A HttpURLConnectionFactory that performs some basic checks to ensure we are uploading
 * minidumps correctly.
 */
public class TestHttpURLConnectionFactory implements HttpURLConnectionFactory {
    String mContentType;

    public TestHttpURLConnectionFactory() {
        mContentType = TestHttpURLConnection.DEFAULT_EXPECTED_CONTENT_TYPE;
    }

    @Override
    public HttpURLConnection createHttpURLConnection(String url) {
        try {
            return new TestHttpURLConnection(new URL(url), mContentType);
        } catch (IOException e) {
            return null;
        }
    }
}
