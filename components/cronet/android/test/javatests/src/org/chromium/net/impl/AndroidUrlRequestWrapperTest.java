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
import org.chromium.net.NativeTestServer;
import org.chromium.net.TestUrlRequestCallback;

/** Tests wrapper specific features. */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.STATICALLY_LINKED},
        reason = "HttpEngine's UrlRequestWrapper specific tests")
public class AndroidUrlRequestWrapperTest {
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
    public void testCallback_twoStreamsFromOneBuilder_streamRecordedCorrectlyUpdated() {
        TestUrlRequestCallback.SimpleSucceedingCallback callback =
                new TestUrlRequestCallback.SimpleSucceedingCallback();

        AndroidUrlRequestBuilderWrapper builder =
                (AndroidUrlRequestBuilderWrapper)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(
                                        NativeTestServer.getSuccessURL(),
                                        callback,
                                        callback.getExecutor());

        AndroidUrlRequestWrapper request1 = (AndroidUrlRequestWrapper) builder.build();
        AndroidUrlRequestWrapper request2 = (AndroidUrlRequestWrapper) builder.build();
        assertThat(builder.getCallback().getRequestRecordCopy()).hasSize(2);

        request1.start();
        callback.done.block();

        assertThat(builder.getCallback().getRequestRecordCopy()).hasSize(1);

        callback.done.close();
        request2.start();
        callback.done.block();

        assertThat(builder.getCallback().getRequestRecordCopy()).isEmpty();
    }
}
