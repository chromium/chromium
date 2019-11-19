// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static org.junit.Assert.assertEquals;

import static org.chromium.net.CronetTestRule.getContext;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetEngine;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.QuicTestServer;

import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;

/**
 * Tests HttpURLConnection upload using QUIC.
 */
@RunWith(AndroidJUnit4.class)
public class QuicUploadTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());

        QuicTestServer.startQuicTestServer(getContext());

        builder.enableQuic(true);
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());

        builder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());

        mCronetEngine = builder.build();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
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
        assertEquals(200, connection.getResponseCode());
        connection.disconnect();
    }
}
