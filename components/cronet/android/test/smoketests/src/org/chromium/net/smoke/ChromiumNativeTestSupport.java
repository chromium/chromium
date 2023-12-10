// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;

import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.ExperimentalCronetEngine;

/** Provides support for tests that depend on QUIC and HTTP2 servers. */
class ChromiumNativeTestSupport extends ChromiumPlatformOnlyTestSupport {
    private static final String TAG = ChromiumNativeTestSupport.class.getSimpleName();

    @Override
    public TestServer createTestServer(Context context, Protocol protocol) {
        switch (protocol) {
            case QUIC:
                return new QuicTestServer(context);
            case HTTP2:
                return new Http2TestServer(context);
            case HTTP1:
                return super.createTestServer(context, protocol);
            default:
                throw new RuntimeException("Unknown server protocol: " + protocol);
        }
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
        System.loadLibrary("cronet_tests");
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

    private static class Http2TestServer implements TestServer {
        private final Context mContext;

        Http2TestServer(Context context) {
            mContext = context;
        }

        @Override
        public boolean start() {
            try {
                return org.chromium.net.Http2TestServer.startHttp2TestServer(mContext);
            } catch (Exception e) {
                Log.e(TAG, "Exception during Http2TestServer start", e);
                return false;
            }
        }

        @Override
        public void shutdown() {
            try {
                org.chromium.net.Http2TestServer.shutdownHttp2TestServer();
            } catch (Exception e) {
                Log.e(TAG, "Exception during Http2TestServer shutdown", e);
            }
        }

        @Override
        public String getSuccessURL() {
            return org.chromium.net.Http2TestServer.getEchoMethodUrl();
        }
    }
}
