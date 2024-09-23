// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.ConditionVariable;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.common.collect.Range;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.UrlRequest.Status;
import org.chromium.net.UrlRequest.StatusListener;
import org.chromium.net.impl.LoadState;
import org.chromium.net.impl.UrlRequestUtil;

import java.io.IOException;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Tests that {@link org.chromium.net.impl.CronetUrlRequest#getStatus(StatusListener)} works as
 * expected.
 */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
public class GetStatusTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();
    @Rule public ExpectedException thrown = ExpectedException.none();

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
    @SmallTest
    public void testSimpleGet() throws Exception {
        String url = NativeTestServer.getEchoMethodURL();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(url, callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();
        // Calling before request is started should give Status.INVALID,
        // since the native adapter is not created.
        TestStatusListener statusListener0 = new TestStatusListener();
        urlRequest.getStatus(statusListener0);
        statusListener0.waitUntilOnStatusCalled();
        assertThat(statusListener0.mOnStatusCalled).isTrue();
        assertThat(statusListener0.mStatus).isEqualTo(Status.INVALID);

        urlRequest.start();

        // Should receive a valid status.
        TestStatusListener statusListener1 = new TestStatusListener();
        urlRequest.getStatus(statusListener1);
        statusListener1.waitUntilOnStatusCalled();
        assertThat(statusListener1.mOnStatusCalled).isTrue();
        assertThat(statusListener1.mStatus)
                .isIn(Range.closed(Status.IDLE, Status.READING_RESPONSE));
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        callback.startNextRead(urlRequest);

        // Should receive a valid status.
        TestStatusListener statusListener2 = new TestStatusListener();
        urlRequest.getStatus(statusListener2);
        statusListener2.waitUntilOnStatusCalled();
        assertThat(statusListener2.mOnStatusCalled).isTrue();
        assertThat(statusListener1.mStatus)
                .isIn(Range.closed(Status.IDLE, Status.READING_RESPONSE));

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);

        callback.startNextRead(urlRequest);
        callback.blockForDone();

        // Calling after request done should give Status.INVALID, since
        // the native adapter is destroyed.
        TestStatusListener statusListener3 = new TestStatusListener();
        urlRequest.getStatus(statusListener3);
        statusListener3.waitUntilOnStatusCalled();
        assertThat(statusListener3.mOnStatusCalled).isTrue();
        assertThat(statusListener3.mStatus).isEqualTo(Status.INVALID);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
    }

    @Test
    @SmallTest
    public void testInvalidLoadState() throws Exception {
        assertThrows(
                IllegalArgumentException.class,
                () -> UrlRequestUtil.convertLoadState(LoadState.OBSOLETE_WAITING_FOR_APPCACHE));
        // Expected throw because LoadState.WAITING_FOR_APPCACHE is not mapped.

        thrown.expect(Throwable.class);
        UrlRequestUtil.convertLoadState(-1);
        UrlRequestUtil.convertLoadState(16);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Relies on timings which are not respected by the fallback implementation")
    // Regression test for crbug.com/606872.
    public void testGetStatusForUpload() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                NativeTestServer.getEchoBodyURL(),
                                callback,
                                callback.getExecutor());

        final ConditionVariable block = new ConditionVariable();
        // Use a separate executor for UploadDataProvider so the upload can be
        // stalled while getStatus gets processed.
        Executor uploadProviderExecutor = Executors.newSingleThreadExecutor();
        TestUploadDataProvider dataProvider =
                new TestUploadDataProvider(
                        TestUploadDataProvider.SuccessCallbackMode.SYNC, uploadProviderExecutor) {
                    @Override
                    public long getLength() throws IOException {
                        // Pause the data provider.
                        block.block();
                        block.close();
                        return super.getLength();
                    }
                };
        dataProvider.addRead("test".getBytes());
        builder.setUploadDataProvider(dataProvider, uploadProviderExecutor);
        builder.addHeader("Content-Type", "useless/string");
        UrlRequest urlRequest = builder.build();
        TestStatusListener statusListener = new TestStatusListener();
        urlRequest.start();
        // Call getStatus() immediately after start(), which will post
        // startInternal() to the upload provider's executor because there is an
        // upload. When CronetUrlRequestAdapter::GetStatusOnNetworkThread is
        // executed, the |url_request_| is null.
        urlRequest.getStatus(statusListener);
        statusListener.waitUntilOnStatusCalled();
        assertThat(statusListener.mOnStatusCalled).isTrue();
        // The request should be in IDLE state because GetStatusOnNetworkThread
        // is called before |url_request_| is initialized and started.
        assertThat(statusListener.mStatus).isEqualTo(Status.IDLE);
        // Resume the UploadDataProvider.
        block.open();

        // Make sure the request is successful and there is no crash.
        callback.blockForDone();
        dataProvider.assertClosed();

        assertThat(dataProvider.getUploadedLength()).isEqualTo(4);
        assertThat(dataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(dataProvider.getNumRewindCalls()).isEqualTo(0);

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("test");
    }

    private static class TestStatusListener extends StatusListener {
        boolean mOnStatusCalled;
        int mStatus = Integer.MAX_VALUE;
        private final ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void onStatus(int status) {
            mOnStatusCalled = true;
            mStatus = status;
            mBlock.open();
        }

        public void waitUntilOnStatusCalled() {
            mBlock.block();
            mBlock.close();
        }
    }
}
