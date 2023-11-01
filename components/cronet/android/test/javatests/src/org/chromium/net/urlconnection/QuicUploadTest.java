// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.QuicTestServer;

import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;

/** Tests HttpURLConnection upload using QUIC. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason =
                "The fallback implementation doesn't support QUIC. "
                        + "crbug.com/1494870: Enable for AOSP_PLATFORM once fixed")
public class QuicUploadTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        QuicTestServer.startQuicTestServer(mTestRule.getTestFramework().getContext());

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            builder.enableQuic(true);
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject().put("HostResolverRules", hostResolverParams);
                            builder.setExperimentalOptions(experimentalOptions.toString());

                            builder.addQuicHint(
                                    QuicTestServer.getServerHost(),
                                    QuicTestServer.getServerPort(),
                                    QuicTestServer.getServerPort());

                            CronetTestUtil.setMockCertVerifierForTesting(
                                    builder, QuicTestServer.createMockCertVerifier());
                        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
    }

    @After
    public void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @SmallTest
    // Regression testing for crbug.com/618872.
    public void testOneMassiveWrite() throws Exception {
        String path = "/simple.txt";
        URL url = new URL(QuicTestServer.getServerURL() + path);
        HttpURLConnection connection = (HttpURLConnection) mCronetEngine.openConnection(url);
        connection.setDoOutput(true);
        connection.setRequestMethod("POST");
        // Size is chosen so the last time mBuffer will be written 14831 bytes,
        // which is larger than the internal QUIC read buffer size of 14520.
        byte[] largeData = new byte[195055];
        Arrays.fill(largeData, "a".getBytes("UTF-8")[0]);
        connection.setFixedLengthStreamingMode(largeData.length);
        OutputStream out = connection.getOutputStream();
        // Write everything at one go, so the data is larger than the buffer
        // used in CronetFixedModeOutputStream.
        out.write(largeData);
        assertThat(connection.getResponseCode()).isEqualTo(200);
        connection.disconnect();
    }
}
