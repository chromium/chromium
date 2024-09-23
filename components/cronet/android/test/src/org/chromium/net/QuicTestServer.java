// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.UrlUtils;

/** Wrapper class to start a Quic test server. */
@JNINamespace("cronet")
public final class QuicTestServer {
    private static final String TAG = QuicTestServer.class.getSimpleName();

    private static final String CERT_USED = "quic-chain.pem";
    private static final String KEY_USED = "quic-leaf-cert.key";
    private static final String[] CERTS_USED = {CERT_USED};

    private static boolean sServerRunning;

    /*
     * Starts the server.
     */
    public static void startQuicTestServer(Context context) {
        if (sServerRunning) {
            throw new IllegalStateException("Quic server is already running");
        }
        TestFilesInstaller.installIfNeeded(context);
        QuicTestServerJni.get()
                .startQuicTestServer(
                        TestFilesInstaller.getInstalledPath(context),
                        UrlUtils.getIsolatedTestRoot());
        sServerRunning = true;
    }

    /** Shuts down the server. No-op if the server is already shut down. */
    public static void shutdownQuicTestServer() {
        if (!sServerRunning) {
            return;
        }
        QuicTestServerJni.get().shutdownQuicTestServer();
        sServerRunning = false;
    }

    public static String getServerURL() {
        return "https://" + getServerHost() + ":" + getServerPort();
    }

    public static String getServerHost() {
        return CronetTestUtil.QUIC_FAKE_HOST;
    }

    public static int getServerPort() {
        return QuicTestServerJni.get().getServerPort();
    }

    public static String getConnectionClosePath() {
        return QuicTestServerJni.get().getConnectionClosePath();
    }

    public static void delayResponse(String path, int delayInSeconds) {
        QuicTestServerJni.get().delayResponse(path, delayInSeconds);
    }

    public static final String getServerCert() {
        return CERT_USED;
    }

    public static final String getServerCertKey() {
        return KEY_USED;
    }

    public static long createMockCertVerifier() {
        TestFilesInstaller.installIfNeeded(ContextUtils.getApplicationContext());
        return MockCertVerifier.createMockCertVerifier(CERTS_USED, true);
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        /*
         * Runs a quic test server synchronously.
         */
        void startQuicTestServer(String filePath, String testDataDir);

        /*
         * Shutdowns the quic test-server synchronously.
         *
         * Calling this without calling startQuicTestServer first will lead to unexpected
         * behavior if not compiled in debug mode.
         */
        void shutdownQuicTestServer();

        int getServerPort();

        /*
         * Responses for path will be delayed by delayInSeconds.
         *
         * Ideally this wouldn't take a delay. Instead, it should provide a synchronization
         * mechanism that allows the caller to unblock the request. This would require changes all
         * the way down to QUICHE though.
         */
        void delayResponse(String path, int delayInSeconds);

        /*
         * The server will send a CONNECTION_CLOSE packet to the client upon receiving a connection
         * on the specified path.
         *
         * The expected error code is QUIC_NO_ERROR.
         */
        String getConnectionClosePath();
    }
}
