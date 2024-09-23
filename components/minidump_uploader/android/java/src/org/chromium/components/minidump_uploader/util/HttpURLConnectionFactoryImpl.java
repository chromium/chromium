// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader.util;

import org.chromium.net.ChromiumNetworkAdapter;
import org.chromium.net.NetworkTrafficAnnotationTag;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

/** Default implementation of HttpURLConnectionFactory. */
public class HttpURLConnectionFactoryImpl implements HttpURLConnectionFactory {
    private static final NetworkTrafficAnnotationTag TRAFFIC_ANNOTATION =
            NetworkTrafficAnnotationTag.createComplete(
                    "minidump_uploader_android",
                    """
                    semantics {
                      sender: "Minidump Uploader (Android)"
                      description:
                        "Uploads crash reports to Google servers. This data is used by the Chrome "
                        "team to track down and fix critical bugs."
                      trigger: "During startup, if a crash occurred recently."
                      data: "Crash report, including a trace and device info."
                      destination: GOOGLE_OWNED_SERVICE
                    }
                    policy {
                      cookies_allowed: NO
                      setting:
                        "Settings > Google Services > Help improve Chrome's features and "
                        "performance."
                      policy_exception_justification:
                        "MetricsReportingEnabled is only implemented on desktop and ChromeOS."
                    }""");

    @Override
    public HttpURLConnection createHttpURLConnection(String url) {
        try {
            return (HttpURLConnection)
                    ChromiumNetworkAdapter.openConnection(new URL(url), TRAFFIC_ANNOTATION);
        } catch (IOException e) {
            return null;
        }
    }
}
