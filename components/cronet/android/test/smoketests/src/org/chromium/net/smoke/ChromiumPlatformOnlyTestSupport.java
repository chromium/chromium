// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;

import org.json.JSONObject;

import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.smoke.TestSupport.Protocol;
import org.chromium.net.smoke.TestSupport.TestServer;

import java.io.File;

/**
 * Tests support for Java only Cronet engine tests. This class should not depend on
 * Chromium 'base' or 'net'.
 */
public class ChromiumPlatformOnlyTestSupport implements TestSupport {
    @Override
    public TestServer createTestServer(Context context, Protocol protocol) {
        switch (protocol) {
            case QUIC:
                throw new IllegalArgumentException("QUIC is not supported");
            case HTTP2:
                throw new IllegalArgumentException("HTTP2 is not supported");
            case HTTP1:
                return new HttpTestServer();
            default:
                throw new IllegalArgumentException("Unknown server protocol: " + protocol);
        }
    }

    @Override
    public void processNetLog(Context context, File file) {
        // Do nothing
    }

    @Override
    public void addHostResolverRules(JSONObject experimentalOptionsJson) {
        throw new UnsupportedOperationException("Unsupported by ChromiumPlatformOnlyTestSupport");
    }

    @Override
    public void installMockCertVerifierForTesting(ExperimentalCronetEngine.Builder builder) {
        throw new UnsupportedOperationException("Unsupported by ChromiumPlatformOnlyTestSupport");
    }

    @Override
    public void loadTestNativeLibrary() {
        throw new UnsupportedOperationException("Unsupported by ChromiumPlatformOnlyTestSupport");
    }
}
