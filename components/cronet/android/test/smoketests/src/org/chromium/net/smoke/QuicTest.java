// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.smoke.TestSupport.Protocol.QUIC;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.UrlRequest;

import java.net.URL;

/** QUIC Tests. */
@RunWith(AndroidJUnit4.class)
public class QuicTest {
    private TestSupport.TestServer mServer;

    @Rule public NativeCronetTestRule mRule = new NativeCronetTestRule();

    @Before
    public void setUp() throws Exception {
        mServer =
                mRule.getTestSupport()
                        .createTestServer(ApplicationProvider.getApplicationContext(), QUIC);
    }

    @After
    public void tearDown() throws Exception {
        mServer.shutdown();
    }

    @Test
    @SmallTest
    public void testQuic() throws Exception {
        assertThat(mServer.start()).isTrue();
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
                    mRule.getCronetEngine()
                            .newUrlRequestBuilder(urlString, callback, callback.getExecutor());
            requestBuilder.build().start();
            callback.blockForDone();
            NativeCronetTestRule.assertSuccessfulNonEmptyResponse(callback, urlString);
            if (callback.getResponseInfo().getNegotiatedProtocol().startsWith("http/2+quic")
                    || callback.getResponseInfo().getNegotiatedProtocol().startsWith("h3")) {
                quicNegotiated = true;
                break;
            }
        }
        assertThat(quicNegotiated).isTrue();
    }
}
