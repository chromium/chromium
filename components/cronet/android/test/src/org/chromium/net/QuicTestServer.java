// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.os.ConditionVariable;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.test.util.UrlUtils;

/**
 * Wrapper class to start a Quic test server.
 */
@JNINamespace("cronet")
public final class QuicTestServer {
    private static final ConditionVariable sBlock = new ConditionVariable();
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
        nativeStartQuicTestServer(
                TestFilesInstaller.getInstalledPath(context), UrlUtils.getIsolatedTestRoot());
        sBlock.block();
        sBlock.close();
        sServerRunning = true;
    }

    /**
     * Shuts down the server. No-op if the server is already shut down.
     */
    public static void shutdownQuicTestServer() {
        if (!sServerRunning) {
            return;
        }
        nativeShutdownQuicTestServer();
        sServerRunning = false;
    }

    public static String getServerURL() {
        return "https://" + getServerHost() + ":" + getServerPort();
    }

    public static String getServerHost() {
        return CronetTestUtil.QUIC_FAKE_HOST;
    }

    public static int getServerPort() {
        return nativeGetServerPort();
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

    @CalledByNative
    private static void onServerStarted() {
        Log.i(TAG, "Quic server started.");
        sBlock.open();
    }

    private static native void nativeStartQuicTestServer(String filePath, String testDataDir);
    private static native void nativeShutdownQuicTestServer();
    private static native int nativeGetServerPort();
}
