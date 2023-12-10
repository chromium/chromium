// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader.util;

import java.net.HttpURLConnection;

/** A factory class for creating a HttpURLConnection. */
public interface HttpURLConnectionFactory {
    /**
     * @param url the url to communicate with
     * @return a HttpURLConnection to communicate with |url|
     */
    HttpURLConnection createHttpURLConnection(String url);
}
