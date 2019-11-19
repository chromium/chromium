// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.UrlRequest;

/**
 * HTTP2 Tests.
 */
@RunWith(AndroidJUnit4.class)
public class Http2Test {
    private TestSupport.TestServer mServer;

    @Rule
    public NativeCronetTestRule mRule = new NativeCronetTestRule();

    @Before
    public void setUp() throws Exception {
        mServer = mRule.getTestSupport().createTestServer(
                InstrumentationRegistry.getTargetContext(), TestSupport.Protocol.HTTP2);
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    // Test that HTTP/2 is enabled by default but QUIC is not.
    @Test
    @SmallTest
    public void testHttp2() throws Exception {
        mRule.getTestSupport().installMockCertVerifierForTesting(mRule.getCronetEngineBuilder());
        mRule.initCronetEngine();
        Assert.assertTrue(mServer.start());
        SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
        UrlRequest.Builder requestBuilder = mRule.getCronetEngine().newUrlRequestBuilder(
                mServer.getSuccessURL(), callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();

        CronetSmokeTestRule.assertSuccessfulNonEmptyResponse(callback, mServer.getSuccessURL());
        Assert.assertEquals("h2", callback.getResponseInfo().getNegotiatedProtocol());
    }
}
