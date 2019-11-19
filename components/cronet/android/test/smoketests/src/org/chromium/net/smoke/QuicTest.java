// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static org.chromium.net.smoke.TestSupport.Protocol.QUIC;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.UrlRequest;

import java.net.URL;

/**
 * QUIC Tests.
 */
@RunWith(AndroidJUnit4.class)
public class QuicTest {
    private TestSupport.TestServer mServer;

    @Rule
    public NativeCronetTestRule mRule = new NativeCronetTestRule();

    @Before
    public void setUp() throws Exception {
        mServer = mRule.getTestSupport().createTestServer(
                InstrumentationRegistry.getTargetContext(), QUIC);
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    @Test
    @SmallTest
    public void testQuic() throws Exception {
        Assert.assertTrue(mServer.start());
        final String urlString = mServer.getSuccessURL();
        final URL url = new URL(urlString);

        mRule.getCronetEngineBuilder().enableQuic(true);
        mRule.getCronetEngineBuilder().addQuicHint(url.getHost(), url.getPort(), url.getPort());
        mRule.getTestSupport().installMockCertVerifierForTesting(mRule.getCronetEngineBuilder());

        JSONObject quicParams = new JSONObject();
        JSONObject experimentalOptions = new JSONObject().put("QUIC", quicParams);
        mRule.getTestSupport().addHostResolverRules(experimentalOptions);
        mRule.getCronetEngineBuilder().setExperimentalOptions(experimentalOptions.toString());

        mRule.initCronetEngine();

        // QUIC is not guaranteed to win the race, so try multiple times.
        boolean quicNegotiated = false;

        for (int i = 0; i < 5; i++) {
            SmokeTestRequestCallback callback = new SmokeTestRequestCallback();
            UrlRequest.Builder requestBuilder =
                    mRule.getCronetEngine().newUrlRequestBuilder(
                            urlString, callback, callback.getExecutor());
            requestBuilder.build().start();
            callback.blockForDone();
            NativeCronetTestRule.assertSuccessfulNonEmptyResponse(callback, urlString);
            if (callback.getResponseInfo().getNegotiatedProtocol().startsWith("http/2+quic/")) {
                quicNegotiated = true;
                break;
            }
        }
        Assert.assertTrue(quicNegotiated);
    }
}
