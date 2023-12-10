// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import org.chromium.net.CronetException;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.RequestFinishedInfoImpl;

import java.util.Collection;

/** Extension of {@link RequestFinishedInfoImpl} which does not support metrics for testing purposes. */
public class FakeRequestFinishedInfo extends RequestFinishedInfoImpl {
    public FakeRequestFinishedInfo(
            String url,
            Collection<Object> connectionAnnotations,
            int finishedReason,
            UrlResponseInfo responseInfo,
            CronetException exception) {
        super(url, connectionAnnotations, null, finishedReason, responseInfo, exception);
    }

    @Override
    public Metrics getMetrics() {
        throw new UnsupportedOperationException("Metrics are not supported for fake requests.");
    }
}
