// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.test.FailurePhase;

/** Helper class to set up url interceptors for testing purposes. */
@JNINamespace("cronet")
public final class MockUrlRequestJobFactory {
    private final long mInterceptorHandle;
    private final CronetTestUtil.NetworkThreadTestConnector mNetworkThreadTestConnector;

    /** Sets up URL interceptors. */
    public MockUrlRequestJobFactory(CronetEngine cronetEngine) {
        mNetworkThreadTestConnector = new CronetTestUtil.NetworkThreadTestConnector(cronetEngine);

        mInterceptorHandle =
                MockUrlRequestJobFactoryJni.get()
                        .addUrlInterceptors(
                                ((CronetUrlRequestContext) cronetEngine)
                                        .getUrlRequestContextAdapter());
    }

    /** Remove URL Interceptors. */
    public void shutdown() {
        MockUrlRequestJobFactoryJni.get().removeUrlInterceptorJobFactory(mInterceptorHandle);
        mNetworkThreadTestConnector.shutdown();
    }

    /**
     * Constructs a mock URL that hangs or fails at certain phase.
     *
     * @param phase at which request fails. It should be a value in
     *              org.chromium.net.test.FailurePhase.
     * @param netError reported by UrlRequestJob. Passing -1, results in hang.
     */
    public static String getMockUrlWithFailure(int phase, int netError) {
        assertThat(netError).isLessThan(0);
        switch (phase) {
            case FailurePhase.START:
            case FailurePhase.READ_SYNC:
            case FailurePhase.READ_ASYNC:
                break;
            default:
                throw new IllegalArgumentException(
                        "phase not in org.chromium.net.test.FailurePhase");
        }
        return MockUrlRequestJobFactoryJni.get().getMockUrlWithFailure(phase, netError);
    }

    /**
     * Constructs a mock URL that synchronously responds with data repeated many
     * times.
     *
     * @param data to return in response.
     * @param dataRepeatCount number of times to repeat the data.
     */
    public static String getMockUrlForData(String data, int dataRepeatCount) {
        return MockUrlRequestJobFactoryJni.get().getMockUrlForData(data, dataRepeatCount);
    }

    /**
     * Constructs a mock URL that will request client certificate and return
     * the string "data" as the response.
     */
    public static String getMockUrlForClientCertificateRequest() {
        return MockUrlRequestJobFactoryJni.get().getMockUrlForClientCertificateRequest();
    }

    /** Constructs a mock URL that will fail with an SSL certificate error. */
    public static String getMockUrlForSSLCertificateError() {
        return MockUrlRequestJobFactoryJni.get().getMockUrlForSSLCertificateError();
    }

    /** Constructs a mock URL that will hang when try to read response body from the remote. */
    public static String getMockUrlForHangingRead() {
        return MockUrlRequestJobFactoryJni.get().getMockUrlForHangingRead();
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        long addUrlInterceptors(long requestContextAdapter);

        void removeUrlInterceptorJobFactory(long interceptorHandle);

        String getMockUrlWithFailure(int phase, int netError);

        String getMockUrlForData(String data, int dataRepeatCount);

        String getMockUrlForClientCertificateRequest();

        String getMockUrlForSSLCertificateError();

        String getMockUrlForHangingRead();
    }
}
