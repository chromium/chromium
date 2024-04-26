// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.UrlRequest;

/** HTTP2 Tests. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class Http2Test {
    private TestSupport.TestServer mServer;

    @Rule public NativeCronetTestRule mRule = new NativeCronetTestRule();

    @Before
    public void setUp() throws Exception {
        mServer =
                mRule.getTestSupport()
                        .createTestServer(
                                ApplicationProvider.getApplicationContext(),
                                TestSupport.Protocol.HTTP2);
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    // Test that HTTP/2 is enabled by default but QUIC is not.
    @Test
    @SmallTest
    public void testHttp2() throws Exception {
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            mRule.getTestSupport()
                    .installMockCertVerifierForTesting(mRule.getCronetEngineBuilder());
        }
        mRule.initCronetEngine();
        assertThat(mServer.start()).isTrue();
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder =
                mRule.getCronetEngine()
                        .newUrlRequestBuilder(
                                mServer.getSuccessURL(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        CronetSmokeTestRule.assertSuccessfulNonEmptyResponse(callback, mServer.getSuccessURL());
        assertThat(callback.getResponseInfo()).hasNegotiatedProtocolThat().isEqualTo("h2");
    }
}
