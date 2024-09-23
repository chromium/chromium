// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;

import android.os.ConditionVariable;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.MetricsTestUtil.TestExecutor;
import org.chromium.net.impl.CronetMetrics;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Test RequestFinishedInfo.Listener and the metrics information it provides. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "Fallback implementation does not support RequestFinishedListener.")
public class RequestFinishedInfoTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withAutomaticEngineStartup();

    private String mUrl;
    private CronetImplementation mImplementationUnderTest;

    // A subclass of TestRequestFinishedListener to additionally assert that UrlRequest.Callback's
    // terminal callbacks have been invoked at the time of onRequestFinished().
    // See crbug.com/710877.
    private static class AssertCallbackDoneRequestFinishedListener
            extends TestRequestFinishedListener {
        private final TestUrlRequestCallback mCallback;

        public AssertCallbackDoneRequestFinishedListener(TestUrlRequestCallback callback) {
            // Use same executor as request callback to verify stable call order.
            super(callback.getExecutor());
            mCallback = callback;
        }

        @Override
        public void onRequestFinished(RequestFinishedInfo requestInfo) {
            assertThat(mCallback.isDone()).isTrue();
            super.onRequestFinished(requestInfo);
        }
    }

    @Before
    public void setUp() throws Exception {
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());
        mUrl = NativeTestServer.getFileURL("/echo?status=200");
        mImplementationUnderTest = mTestRule.implementationUnderTest();
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    static class DirectExecutor implements Executor {
        private ConditionVariable mBlock = new ConditionVariable();

        @Override
        public void execute(Runnable task) {
            task.run();
            mBlock.open();
        }

        public void blockUntilDone() {
            mBlock.block();
        }
    }

    static class ThreadExecutor implements Executor {
        private List<Thread> mThreads = new ArrayList<Thread>();

        @Override
        public void execute(Runnable task) {
            Thread newThread = new Thread(task);
            mThreads.add(newThread);
            newThread.start();
        }

        public void joinAll() throws InterruptedException {
            for (Thread thread : mThreads) {
                thread.join();
            }
        }
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListener() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);
        assertThat(requestInfo.getAnnotations()).containsExactly("request annotation", this);
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDirectExecutor() throws Exception {
        DirectExecutor testExecutor = new DirectExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        // Block on the executor, not the listener, since blocking on the listener doesn't work when
        // it's created with a non-default executor.
        testExecutor.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);
        assertThat(requestInfo.getAnnotations()).containsExactly("request annotation", this);
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerDifferentThreads() throws Exception {
        TestRequestFinishedListener firstListener = new TestRequestFinishedListener();
        TestRequestFinishedListener secondListener = new TestRequestFinishedListener();
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(firstListener);
        mTestRule.getTestFramework().getEngine().addRequestFinishedListener(secondListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        firstListener.blockUntilDone();
        secondListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo firstRequestInfo = firstListener.getRequestInfo();
        RequestFinishedInfo secondRequestInfo = secondListener.getRequestInfo();

        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, firstRequestInfo, mUrl, startTime, endTime);
        assertThat(firstRequestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, firstRequestInfo.getMetrics(), startTime, endTime, false);

        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, secondRequestInfo, mUrl, startTime, endTime);
        assertThat(secondRequestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest,
                secondRequestInfo.getMetrics(),
                startTime,
                endTime,
                false);

        assertThat(firstRequestInfo.getAnnotations()).containsExactly("request annotation", this);
        assertThat(secondRequestInfo.getAnnotations()).containsExactly("request annotation", this);
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerFailedRequest() throws Exception {
        String connectionRefusedUrl = "http://127.0.0.1:3";
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                connectionRefusedUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertWithMessage("RequestFinishedInfo.Listener must be called")
                .that(requestInfo)
                .isNotNull();
        assertThat(requestInfo.getUrl()).isEqualTo(connectionRefusedUrl);
        assertThat(requestInfo.getAnnotations()).isEmpty();
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.FAILED);
        assertThat(requestInfo.getException()).isNotNull();
        assertThat(((NetworkException) requestInfo.getException()).getErrorCode())
                .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertWithMessage("RequestFinishedInfo.getMetrics() must not be null")
                .that(metrics)
                .isNotNull();

        // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
        // metrics. Don't bother checking timing metrics for AOSP whether it passes or not.
        if (mImplementationUnderTest != CronetImplementation.AOSP_PLATFORM) {
            // The failure is occasionally fast enough that time reported is 0, so just check for
            // null
            assertThat(metrics.getTotalTimeMs()).isNotNull();
            assertThat(metrics.getTtfbMs()).isNull();

            // Check the timing metrics
            assertThat(metrics.getRequestStart()).isNotNull();
            MetricsTestUtil.assertAfter(metrics.getRequestStart(), startTime);
            MetricsTestUtil.checkNoConnectTiming(mImplementationUnderTest, metrics);
            assertThat(metrics.getSendingStart()).isNull();
            assertThat(metrics.getSendingEnd()).isNull();
            assertThat(metrics.getResponseStart()).isNull();
            assertThat(metrics.getRequestEnd()).isNotNull();
            MetricsTestUtil.assertAfter(endTime, metrics.getRequestEnd());
            MetricsTestUtil.assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
            assertThat(metrics.getSentByteCount()).isEqualTo(0);
            assertThat(metrics.getReceivedByteCount()).isEqualTo(0);
        }
    }

    @Test
    @SmallTest
    public void testRequestFinishedListenerThrowInTerminalCallback() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setFailure(
                TestUrlRequestCallback.FailureType.THROW_SYNC,
                TestUrlRequestCallback.ResponseStep.ON_SUCCEEDED);
        mTestRule
                .getTestFramework()
                .getEngine()
                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
    }

    @Test
    @SmallTest
    public void testRequestFinishedListenerThrowInListener() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.makeListenerThrow();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        mTestRule
                .getTestFramework()
                .getEngine()
                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
                .setRequestFinishedListener(requestFinishedListener)
                .build()
                .start();
        callback.blockForDone();
        // We expect that the exception from the listener will not crash the test.
        requestFinishedListener.blockUntilDone();
    }

    @Test
    @SmallTest
    public void testRequestFinishedListenerThrowInEngineListener() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.makeListenerThrow();
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        mTestRule
                .getTestFramework()
                .getEngine()
                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
                .build()
                .start();
        callback.blockForDone();
        // We expect that the exception from the listener will not crash the test.
        requestFinishedListener.blockUntilDone();
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testRequestFinishedListenerRemoved() throws Exception {
        TestExecutor testExecutor = new TestExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(testExecutor);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest request = urlRequestBuilder.build();
        mTestRule
                .getTestFramework()
                .getEngine()
                .removeRequestFinishedListener(requestFinishedListener);
        request.start();
        callback.blockForDone();
        testExecutor.runAllTasks();

        assertWithMessage("RequestFinishedInfo.Listener must not be called")
                .that(requestFinishedListener.getRequestInfo())
                .isNull();
    }

    @Test
    @SmallTest
    public void testRequestFinishedListenerCanceledRequest() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback =
                new TestUrlRequestCallback() {
                    @Override
                    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                        super.onResponseStarted(request, info);
                        request.cancel();
                    }
                };
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.CANCELED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);

        assertThat(requestInfo.getAnnotations()).containsExactly("request annotation", this);
    }

    private static class RejectAllTasksExecutor implements Executor {
        @Override
        public void execute(Runnable task) {
            throw new RejectedExecutionException();
        }
    }

    // Checks that CronetURLRequestAdapter::DestroyOnNetworkThread() doesn't crash when metrics
    // collection is enabled and the URLRequest hasn't been created. See http://crbug.com/675629.
    @Test
    @SmallTest
    public void testExceptionInRequestStart() throws Exception {
        // The listener in this test shouldn't get any tasks.
        Executor executor = new RejectAllTasksExecutor();
        TestRequestFinishedListener requestFinishedListener =
                new TestRequestFinishedListener(executor);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        // Empty headers are invalid and will cause start() to throw an exception.
        UrlRequest request = urlRequestBuilder.addHeader("", "").build();
        IllegalArgumentException e = assertThrows(IllegalArgumentException.class, request::start);
        if (mImplementationUnderTest == CronetImplementation.AOSP_PLATFORM
                && !mTestRule.isRunningInAOSP()) {
            // TODO(b/307234565): Remove check once chromium Android 14 emulator has latest changes.
            assertThat(e).hasMessageThat().isEqualTo("Invalid header =");
        } else {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header with headername: ");
        }
    }

    @Test
    @SmallTest
    public void testMetricsGetters() throws Exception {
        long requestStart = 1;
        long dnsStart = 2;
        long dnsEnd = -1;
        long connectStart = 4;
        long connectEnd = 5;
        long sslStart = 6;
        long sslEnd = 7;
        long sendingStart = 8;
        long sendingEnd = 9;
        long pushStart = 10;
        long pushEnd = 11;
        long responseStart = 12;
        long requestEnd = 13;
        boolean socketReused = true;
        long sentByteCount = 14;
        long receivedByteCount = 15;
        // Make sure nothing gets reordered inside the Metrics class
        RequestFinishedInfo.Metrics metrics =
                new CronetMetrics(
                        requestStart,
                        dnsStart,
                        dnsEnd,
                        connectStart,
                        connectEnd,
                        sslStart,
                        sslEnd,
                        sendingStart,
                        sendingEnd,
                        pushStart,
                        pushEnd,
                        responseStart,
                        requestEnd,
                        socketReused,
                        sentByteCount,
                        receivedByteCount);
        assertThat(metrics.getRequestStart()).isEqualTo(new Date(requestStart));
        // -1 timestamp should translate to null
        assertThat(metrics.getDnsEnd()).isNull();
        assertThat(metrics.getDnsStart()).isEqualTo(new Date(dnsStart));
        assertThat(metrics.getConnectStart()).isEqualTo(new Date(connectStart));
        assertThat(metrics.getConnectEnd()).isEqualTo(new Date(connectEnd));
        assertThat(metrics.getSslStart()).isEqualTo(new Date(sslStart));
        assertThat(metrics.getSslEnd()).isEqualTo(new Date(sslEnd));
        assertThat(metrics.getPushStart()).isEqualTo(new Date(pushStart));
        assertThat(metrics.getPushEnd()).isEqualTo(new Date(pushEnd));
        assertThat(metrics.getResponseStart()).isEqualTo(new Date(responseStart));
        assertThat(metrics.getRequestEnd()).isEqualTo(new Date(requestEnd));
        assertThat(metrics.getSocketReused()).isEqualTo(socketReused);
        assertThat(metrics.getSentByteCount()).isEqualTo(sentByteCount);
        assertThat(metrics.getReceivedByteCount()).isEqualTo(receivedByteCount);
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testOrderSuccessfulRequest() throws Exception {
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);
        assertThat(requestInfo.getAnnotations()).containsExactly("request annotation", this);
    }

    @Test
    @SmallTest
    @RequiresMinApi(11)
    public void testUpdateAnnotationOnSucceeded() throws Exception {
        // The annotation that is updated in onSucceeded() callback.
        AtomicBoolean requestAnnotation = new AtomicBoolean(false);
        final TestUrlRequestCallback callback =
                new TestUrlRequestCallback() {
                    @Override
                    public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                        // Add processing information to request annotation.
                        requestAnnotation.set(true);
                        super.onSucceeded(request, info);
                    }
                };
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                (ExperimentalUrlRequest.Builder)
                        mTestRule
                                .getTestFramework()
                                .getEngine()
                                .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation(requestAnnotation)
                .setRequestFinishedListener(requestFinishedListener)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);
        // Check that annotation got updated in onSucceeded() callback.
        assertThat(requestInfo.getAnnotations()).containsExactly(requestAnnotation);
        assertThat(requestAnnotation.get()).isTrue();
    }

    @Test
    @SmallTest
    // Tests a failed request where the error originates from Java.
    public void testOrderFailedRequestJava() throws Exception {
        final TestUrlRequestCallback callback =
                new TestUrlRequestCallback() {
                    @Override
                    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                        throw new RuntimeException("make this request fail");
                    }
                };
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        UrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        requestFinishedListener.blockUntilDone();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertWithMessage("RequestFinishedInfo.Listener must be called")
                .that(requestInfo)
                .isNotNull();
        assertThat(requestInfo.getUrl()).isEqualTo(mUrl);
        assertThat(requestInfo.getAnnotations()).isEmpty();
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.FAILED);
        assertThat(requestInfo.getException())
                .hasMessageThat()
                .isEqualTo("Exception received from UrlRequest.Callback");
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertWithMessage("RequestFinishedInfo.getMetrics() must not be null")
                .that(metrics)
                .isNotNull();
    }

    @Test
    @SmallTest
    // Tests a failed request where the error originates from native code.
    public void testOrderFailedRequestNative() throws Exception {
        String connectionRefusedUrl = "http://127.0.0.1:3";
        final TestUrlRequestCallback callback = new TestUrlRequestCallback();
        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        UrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(
                                connectionRefusedUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        requestFinishedListener.blockUntilDone();
        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        assertWithMessage("RequestFinishedInfo.Listener must be called")
                .that(requestInfo)
                .isNotNull();
        assertThat(requestInfo.getUrl()).isEqualTo(connectionRefusedUrl);
        assertThat(requestInfo.getAnnotations()).isEmpty();
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.FAILED);
        assertThat(requestInfo.getException()).isNotNull();
        assertThat(((NetworkException) requestInfo.getException()).getErrorCode())
                .isEqualTo(NetworkException.ERROR_CONNECTION_REFUSED);
        RequestFinishedInfo.Metrics metrics = requestInfo.getMetrics();
        assertWithMessage("RequestFinishedInfo.getMetrics() must not be null")
                .that(metrics)
                .isNotNull();
    }

    @Test
    @SmallTest
    public void testOrderCanceledRequest() throws Exception {
        final TestUrlRequestCallback callback =
                new TestUrlRequestCallback() {
                    @Override
                    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                        super.onResponseStarted(request, info);
                        request.cancel();
                    }
                };

        TestRequestFinishedListener requestFinishedListener =
                new AssertCallbackDoneRequestFinishedListener(callback);
        mTestRule
                .getTestFramework()
                .getEngine()
                .addRequestFinishedListener(requestFinishedListener);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        Date startTime = new Date();
        urlRequestBuilder
                .addRequestAnnotation("request annotation")
                .addRequestAnnotation(this)
                .build()
                .start();
        callback.blockForDone();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();

        RequestFinishedInfo requestInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                mImplementationUnderTest, requestInfo, mUrl, startTime, endTime);
        assertThat(requestInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.CANCELED);
        MetricsTestUtil.checkHasConnectTiming(
                mImplementationUnderTest, requestInfo.getMetrics(), startTime, endTime, false);

        assertThat(requestInfo.getAnnotations()).containsExactly("request annotation", this);
    }
}
