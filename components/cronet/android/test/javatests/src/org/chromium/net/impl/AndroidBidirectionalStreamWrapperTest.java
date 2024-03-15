// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.Http2TestServer;
import org.chromium.net.TestBidirectionalStreamCallback;

/** Tests wrapper specific features. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.STATICALLY_LINKED},
        reason = "HttpEngine's bidistreamwrapper specific tests")
public class AndroidBidirectionalStreamWrapperTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    @Before
    public void setUp() throws Exception {
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    public void testCallback_twoStreamsFromOneBuilder_streamsRecordedCorrectlyUpdated() {
        TestBidirectionalStreamCallback.SimpleSucceedingCallback callback =
                new TestBidirectionalStreamCallback.SimpleSucceedingCallback();

        AndroidBidirectionalStreamBuilderWrapper builder =
                (AndroidBidirectionalStreamBuilderWrapper)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newBidirectionalStreamBuilder(
                                        Http2TestServer.getServerUrl(),
                                        callback,
                                        callback.getExecutor())
                                .setHttpMethod("GET");

        AndroidBidirectionalStreamWrapper stream1 =
                (AndroidBidirectionalStreamWrapper) builder.build();
        AndroidBidirectionalStreamWrapper stream2 =
                (AndroidBidirectionalStreamWrapper) builder.build();
        assertThat(builder.getCallback().getStreamRecordCopy()).hasSize(2);

        stream1.start();
        callback.done.block();

        assertThat(builder.getCallback().getStreamRecordCopy()).hasSize(1);

        callback.done.close();
        stream2.start();
        callback.done.block();

        assertThat(builder.getCallback().getStreamRecordCopy()).isEmpty();
    }
}
