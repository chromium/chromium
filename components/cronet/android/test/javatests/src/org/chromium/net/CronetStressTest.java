// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.apihelpers.UploadDataProviders;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Tests that making a large number of requests do not lead to crashes. */
@RunWith(AndroidJUnit4.class)
@DoNotBatch(reason = "crbug/1459563")
// TODO(crbug.com/344675719): Failing when batched, batch this again.
public class CronetStressTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    @Before
    public void setUp() throws Exception {
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    @Test
    @LargeTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "This test crashes the fallback implementation.")
    public void testLargeNumberOfUploads() throws Exception {
        Random random = new Random();
        final int kNumRequest = 1000;
        final int kNumRequestHeaders = 100;
        final int kNumUploadBytes = 1000;
        final byte[] b = new byte[kNumUploadBytes * kNumRequest];
        random.nextBytes(b);
        List<TestUrlRequestCallback> callbacks = new ArrayList<>(kNumRequest);

        ExecutorService callbackExecutor = Executors.newSingleThreadExecutor();
        try {
            for (int i = 0; i < kNumRequest; i++) {
                TestUrlRequestCallback callback = new TestUrlRequestCallback(callbackExecutor);
                UrlRequest.Builder builder =
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(
                                        NativeTestServer.getEchoAllHeadersURL(),
                                        callback,
                                        callback.getExecutor());
                for (int j = 0; j < kNumRequestHeaders; j++) {
                    builder.addHeader("header" + j, Integer.toString(j));
                }
                builder.addHeader("content-type", "useless/string");
                builder.setUploadDataProvider(
                        UploadDataProviders.create(b, i * kNumUploadBytes, kNumUploadBytes),
                        callback.getExecutor());
                UrlRequest request = builder.build();
                request.start();
                callbacks.add(callback);
            }

            for (TestUrlRequestCallback callback : callbacks) {
                callback.blockForDone();
                assertThat(callback.getResponseInfoWithChecks())
                        .hasHttpStatusCodeThat()
                        .isEqualTo(200);
            }
        } finally {
            callbackExecutor.shutdown();
        }
    }
}
