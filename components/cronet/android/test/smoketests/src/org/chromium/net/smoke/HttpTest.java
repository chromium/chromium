// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.Http2TestServer;
import org.chromium.net.UrlRequest;

/**
 * HTTP Tests.
 *
 * <p>This test doesn't explicitly depend on any testing infrastructure that requires JNI'ing into
 * native test-only code, it is used to verify that the production libcroneet.so works as expected.
 */
@DoNotBatch(reason = "This test needs to load a different .so than other tests.")
@RunWith(AndroidJUnit4.class)
public class HttpTest {
    @Before
    public void setUp() throws Exception {
        Http2TestServer.startHttp2TestServer(ApplicationProvider.getApplicationContext());
    }

    @After
    public void tearDown() throws Exception {
        Http2TestServer.shutdownHttp2TestServer();
    }

    @Test
    @SmallTest
    public void testHttp2() throws Exception {
        CronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(ApplicationProvider.getApplicationContext());
        // MockCertVerifier can't be used in this context because we're not bundling
        // libcronet_test.so as we wish to only test the prod shared library.
        assumeTrue(
                "Custom CAs are not supported for Android M and MockCertVerifier can't be used "
                        + "in this content",
                Build.VERSION.SDK_INT > Build.VERSION_CODES.M);
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not
        // supported.
        CronetEngine engine = builder.build();
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder =
                engine.newUrlRequestBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        CronetSmokeTestRule.assertSuccessfulNonEmptyResponse(
                callback, Http2TestServer.getEchoMethodUrl());
        assertThat(callback.getResponseInfo()).hasNegotiatedProtocolThat().isEqualTo("h2");
    }
}
