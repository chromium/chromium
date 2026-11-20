// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;

import org.json.JSONObject;

import org.chromium.net.CronetTestUtil;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.impl.CronetLibraryLoader;

/** Provides support for tests that depend on a QUIC server. */
class ChromiumNativeTestSupport extends ChromiumPlatformOnlyTestSupport {
    @Override
    public TestServer createTestServer(Context context, Protocol protocol) {
        if (protocol == Protocol.QUIC) return new QuicTestServer(context);
        return super.createTestServer(context, protocol);
    }

    @Override
    public void addHostResolverRules(JSONObject experimentalOptionsJson) {
        try {
            JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
            experimentalOptionsJson.put("HostResolverRules", hostResolverParams);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void installMockCertVerifierForTesting(ExperimentalCronetEngine.Builder builder) {
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, org.chromium.net.QuicTestServer.createMockCertVerifier());
    }

    @Override
    public void loadTestNativeLibrary() {
        CronetLibraryLoader.switchToTestLibrary();
        CronetLibraryLoader.loadLibrary();
    }

    private static class QuicTestServer implements TestServer {
        private final Context mContext;

        QuicTestServer(Context context) {
            mContext = context;
        }

        @Override
        public boolean start() {
            org.chromium.net.QuicTestServer.startQuicTestServer(mContext);
            return true;
        }

        @Override
        public void shutdown() {
            org.chromium.net.QuicTestServer.shutdownQuicTestServer();
        }

        @Override
        public String getSuccessURL() {
            return org.chromium.net.QuicTestServer.getServerURL() + "/simple.txt";
        }
    }
}
