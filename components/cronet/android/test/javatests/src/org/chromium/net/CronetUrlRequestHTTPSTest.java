// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeFalse;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.test.ServerCertificate;

/** Test functionality of CronetUrlRequest when SSL is enabled. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class CronetUrlRequestHTTPSTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    @Before
    public void assumeCanInjectTestCertificates() {
        // The fallback implementation does not go through X509Util, so X509Util-based test
        // certificate injection will not work. So we have to rely on the trust anchors configured
        // in the Network Security Config (see res/xml/network_security_config.xml), but that's only
        // supported from API 24 (N). Therefore, if we are running on API <24, there is no way for
        // us to inject a test certificate that will work with the fallback implementation.
        assumeFalse(
                mTestRule.implementationUnderTest()
                                == CronetTestFramework.CronetImplementation.FALLBACK
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.N);
    }

    @Test
    @SmallTest
    public void testSSL() throws Exception {
        try (var nativeTestServer =
                NativeTestServer.createNativeTestServerWithHTTPS(
                        mTestRule.getTestFramework().getContext(), ServerCertificate.CERT_OK)) {
            nativeTestServer.start();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            mTestRule
                    .getTestFramework()
                    .getEngine()
                    .newUrlRequestBuilder(
                            nativeTestServer.getEchoMethodURL(), callback, callback.getExecutor())
                    .build()
                    .start();
            callback.blockForDone();
            assertThat(callback.mResponseAsString).isEqualTo("GET");
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "crbug.com/1495320: Refactor error checking")
    public void testSSLCertificateError() throws Exception {
        try (var nativeTestServer =
                NativeTestServer.createNativeTestServerWithHTTPS(
                        mTestRule.getTestFramework().getContext(),
                        ServerCertificate.CERT_EXPIRED)) {
            nativeTestServer.start();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            mTestRule
                    .getTestFramework()
                    .getEngine()
                    .newUrlRequestBuilder(
                            nativeTestServer.getEchoMethodURL(), callback, callback.getExecutor())
                    .build()
                    .start();
            callback.blockForDone();

            assertThat(callback.getResponseInfo()).isNull();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError)
                    .hasMessageThat()
                    .contains("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID");
            mTestRule.assertCronetInternalErrorCode((NetworkException) callback.mError, -201);
            assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        }
    }

    /**
     * Tests that an SSL cert error with upload will be reported via {@link
     * UrlRequest.Callback#onFailed}.
     */
    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "crbug.com/1495320: Refactor error checking")
    public void testSSLCertificateErrorWithUpload() throws Exception {
        try (var nativeTestServer =
                NativeTestServer.createNativeTestServerWithHTTPS(
                        mTestRule.getTestFramework().getContext(),
                        ServerCertificate.CERT_EXPIRED)) {
            nativeTestServer.start();
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder builder =
                    mTestRule
                            .getTestFramework()
                            .getEngine()
                            .newUrlRequestBuilder(
                                    nativeTestServer.getFileURL("/"),
                                    callback,
                                    callback.getExecutor());

            TestUploadDataProvider dataProvider =
                    new TestUploadDataProvider(
                            TestUploadDataProvider.SuccessCallbackMode.SYNC,
                            callback.getExecutor());
            dataProvider.addRead("test".getBytes());
            builder.setUploadDataProvider(dataProvider, callback.getExecutor());
            builder.addHeader("Content-Type", "useless/string");
            builder.build().start();
            callback.blockForDone();

            assertThat(callback.getResponseInfo()).isNull();
            assertThat(callback.mOnErrorCalled).isTrue();
            assertThat(callback.mError)
                    .hasMessageThat()
                    .contains("Exception in CronetUrlRequest: net::ERR_CERT_DATE_INVALID");
            mTestRule.assertCronetInternalErrorCode((NetworkException) callback.mError, -201);
            assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        }
    }
}
