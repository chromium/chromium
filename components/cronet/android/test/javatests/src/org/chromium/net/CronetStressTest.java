// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.getContext;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;

/**
 * Tests that making a large number of requests do not lead to crashes.
 */
@RunWith(AndroidJUnit4.class)
public class CronetStressTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();
    private CronetTestFramework mTestFramework;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        assertTrue(NativeTestServer.startNativeTestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
        mTestFramework.mCronetEngine.shutdown();
    }

    @Test
    @LargeTest
    @OnlyRunNativeCronet
    public void testLargeNumberOfUploads() throws Exception {
        final int kNumRequest = 1000;
        final int kNumRequestHeaders = 100;
        final int kNumUploadBytes = 1000;
        final byte[] b = new byte[kNumUploadBytes];
        for (int i = 0; i < kNumRequest; i++) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                    NativeTestServer.getEchoAllHeadersURL(), callback, callback.getExecutor());
            for (int j = 0; j < kNumRequestHeaders; j++) {
                builder.addHeader("header" + j, Integer.toString(j));
            }
            builder.addHeader("content-type", "useless/string");
            builder.setUploadDataProvider(
                    UploadDataProviders.create(b, 0, kNumUploadBytes), callback.getExecutor());
            UrlRequest request = builder.build();
            request.start();
            callback.blockForDone();
            callback.shutdownExecutor();
            assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        }
    }
}
