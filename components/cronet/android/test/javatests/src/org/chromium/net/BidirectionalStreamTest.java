// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.net.Network;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate;
import org.chromium.net.TestBidirectionalStreamCallback.FailureType;
import org.chromium.net.TestBidirectionalStreamCallback.ResponseStep;
import org.chromium.net.impl.BidirectionalStreamNetworkException;
import org.chromium.net.impl.CronetBidirectionalStream;
import org.chromium.net.impl.CronetExceptionImpl;
import org.chromium.net.impl.NetworkExceptionImpl;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Test functionality of BidirectionalStream interface. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation doesn't support bidirectional streaming")
public class BidirectionalStreamTest {
    private static final String TAG = BidirectionalStreamTest.class.getSimpleName();

    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private ExperimentalCronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            mTestRule
                    .getTestFramework()
                    .applyEngineBuilderPatch(
                            (builder) ->
                                    CronetTestUtil.setMockCertVerifierForTesting(
                                            builder, QuicTestServer.createMockCertVerifier()));
        }
        mCronetEngine = mTestRule.getTestFramework().startEngine();
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    private static void checkResponseInfo(
            UrlResponseInfo responseInfo,
            String expectedUrl,
            int expectedHttpStatusCode,
            String expectedHttpStatusText) {
        assertThat(responseInfo).hasUrlThat().isEqualTo(expectedUrl);
        assertThat(responseInfo).hasUrlChainThat().containsExactly(expectedUrl);
        assertThat(responseInfo).hasHttpStatusCodeThat().isEqualTo(expectedHttpStatusCode);
        assertThat(responseInfo).hasHttpStatusTextThat().isEqualTo(expectedHttpStatusText);
        assertThat(responseInfo).wasNotCached();
        assertThat(responseInfo.toString()).isNotEmpty();
    }

    private static String createLongString(String base, int repetition) {
        StringBuilder builder = new StringBuilder(base.length() * repetition);
        for (int i = 0; i < repetition; ++i) {
            builder.append(i);
            builder.append(base);
        }
        return builder.toString();
    }

    private static UrlResponseInfo createUrlResponseInfo(
            String[] urls, String message, int statusCode, int receivedBytes, String... headers) {
        ArrayList<Map.Entry<String, String>> headersList = new ArrayList<>();
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(
                    new AbstractMap.SimpleImmutableEntry<String, String>(
                            headers[i], headers[i + 1]));
        }
        UrlResponseInfoImpl urlResponseInfo =
                new UrlResponseInfoImpl(
                        Arrays.asList(urls),
                        statusCode,
                        message,
                        headersList,
                        false,
                        "h2",
                        null,
                        receivedBytes);
        return urlResponseInfo;
    }

    private void runGetWithExpectedReceivedByteCount(int expectedReceivedBytes) throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        requestFinishedListener.blockUntilDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        UrlResponseInfo urlResponseInfo =
                createUrlResponseInfo(
                        new String[] {url}, "", 200, expectedReceivedBytes, ":status", "200");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.getResponseInfoWithChecks());
        checkResponseInfo(
                callback.getResponseInfoWithChecks(), Http2TestServer.getEchoMethodUrl(), 200, "");
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        assertThat(finishedInfo.getAnnotations()).isEmpty();
    }

    @Test
    @SmallTest
    public void testBuilderCheck() throws Exception {
        ExperimentalCronetEngine engine = mTestRule.getTestFramework().getEngine();
        if (mTestRule.testingJavaImpl()) {
            runBuilderCheckJavaImpl(engine);
        } else {
            runBuilderCheckNativeImpl(engine);
        }
    }

    private static void runBuilderCheckNativeImpl(ExperimentalCronetEngine engine)
            throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();

        NullPointerException e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                engine.newBidirectionalStreamBuilder(
                                        null, callback, callback.getExecutor()));
        assertThat(e).hasMessageThat().isEqualTo("URL is required.");

        e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                engine.newBidirectionalStreamBuilder(
                                        Http2TestServer.getServerUrl(),
                                        null,
                                        callback.getExecutor()));
        assertThat(e).hasMessageThat().isEqualTo("Callback is required.");

        e =
                assertThrows(
                        NullPointerException.class,
                        () ->
                                engine.newBidirectionalStreamBuilder(
                                        Http2TestServer.getServerUrl(), callback, null));
        assertThat(e).hasMessageThat().isEqualTo("Executor is required.");

        // Verify successful creation doesn't throw.
        BidirectionalStream.Builder builder =
                engine.newBidirectionalStreamBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());

        e = assertThrows(NullPointerException.class, () -> builder.addHeader(null, "value"));
        assertThat(e).hasMessageThat().isEqualTo("Invalid header name.");
        e = assertThrows(NullPointerException.class, () -> builder.addHeader("name", null));
        assertThat(e).hasMessageThat().isEqualTo("Invalid header value.");
        e = assertThrows(NullPointerException.class, () -> builder.setHttpMethod(null));
        assertThat(e).hasMessageThat().isEqualTo("Method is required.");
    }

    private void runBuilderCheckJavaImpl(ExperimentalCronetEngine engine) {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        assertThrows(
                "JavaCronetEngine doesn't support BidirectionalStream.",
                UnsupportedOperationException.class,
                () ->
                        engine.newBidirectionalStreamBuilder(
                                Http2TestServer.getServerUrl(), callback, callback.getExecutor()));
    }

    @Test
    @SmallTest
    public void testFailPlainHttp() throws Exception {
        String url = "http://example.com";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in BidirectionalStream: net::ERR_DISALLOWED_URL_SCHEME");
        mTestRule.assertCronetInternalErrorCode((NetworkException) callback.mError, -301);
    }

    @Test
    @SmallTest
    public void testSimpleGet() throws Exception {
        // Since this is the first request on the connection, the expected received bytes count
        // must account for an HPACK dynamic table size update.
        int expectedReceivedBytes = 31;

        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        UrlResponseInfo urlResponseInfo =
                createUrlResponseInfo(
                        new String[] {url}, "", 200, expectedReceivedBytes, ":status", "200");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.getResponseInfoWithChecks());
        checkResponseInfo(
                callback.getResponseInfoWithChecks(), Http2TestServer.getEchoMethodUrl(), 200, "");
    }

    @Test
    @SmallTest
    public void testSimpleHead() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("HEAD")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("HEAD");
        UrlResponseInfo urlResponseInfo =
                createUrlResponseInfo(new String[] {url}, "", 200, 32, ":status", "200");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.getResponseInfoWithChecks());
        checkResponseInfo(
                callback.getResponseInfoWithChecks(), Http2TestServer.getEchoMethodUrl(), 200, "");
    }

    @Test
    @SmallTest
    public void testSimplePost() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .addRequestAnnotation(this)
                        .addRequestAnnotation("request annotation")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    public void testPostWithFinishedListener() throws Exception {
        CronetImplementation implementationUnderTest = mTestRule.implementationUnderTest();
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .addRequestAnnotation(this)
                        .addRequestAnnotation("request annotation")
                        .build();
        Date startTime = new Date();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                implementationUnderTest, finishedInfo, url, startTime, endTime);
        assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                implementationUnderTest, finishedInfo.getMetrics(), startTime, endTime, true);
        assertThat(finishedInfo.getAnnotations()).containsExactly("request annotation", this);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCount() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        callback.setBlockOnTerminalState(true);
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(
                                Http2TestServer.getEchoStreamUrl(),
                                callback,
                                callback.getExecutor())
                        .build();
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(0);
        stream.start();
        callback.blockForDone();
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.setBlockOnTerminalState(false);
        waitForActiveRequestCount(0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountWithInvalidRequest() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(
                                Http2TestServer.getEchoStreamUrl(),
                                callback,
                                callback.getExecutor())
                        .addHeader("", "") // Deliberately invalid
                        .build();
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(0);
        assertThrows(IllegalArgumentException.class, stream::start);
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(0);
    }

    @Test
    @SmallTest
    public void testSimpleGetWithCombinedHeader() throws Exception {
        String url = Http2TestServer.getCombinedHeadersUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("foo", Arrays.asList("bar", "bar2"));
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlush() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes(), false);
        callback.addWriteData("1234567890".getBytes(), false);
        callback.addWriteData("woot!".getBytes(), true);
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        // Flush before stream is started should not crash.
        stream.flush();

        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();

        // Flush after stream is completed is no-op. It shouldn't call into the destroyed adapter.
        stream.flush();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "crbug.com/1494845: Requires access to internals not available in AOSP")
    // Tests that a delayed flush() only sends buffers that have been written
    // before it is called, and it doesn't flush buffers in mPendingQueue.
    public void testFlushData() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        final ConditionVariable waitOnStreamReady = new ConditionVariable();
        final ConditionVariable waitForHeaders = new ConditionVariable();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    // Number of onWriteCompleted callbacks that have been invoked.
                    private int mNumWriteCompleted;

                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        mResponseStep = ResponseStep.ON_STREAM_READY;
                        waitOnStreamReady.open();
                    }

                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        super.onResponseHeadersReceived(stream, info);
                        waitForHeaders.open();
                    }

                    @Override
                    public void onWriteCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer buffer,
                            boolean endOfStream) {
                        super.onWriteCompleted(stream, info, buffer, endOfStream);
                        mNumWriteCompleted++;
                        if (mNumWriteCompleted <= 3) {
                            // "6" is in pending queue.
                            List<ByteBuffer> pendingData =
                                    ((CronetBidirectionalStream) stream).getPendingDataForTesting();
                            ByteBuffer pendingBuffer = pendingData.get(0);
                            byte[] content = new byte[pendingBuffer.remaining()];
                            pendingBuffer.get(content);
                            assertThat(content).isEqualTo("6".getBytes());

                            // "4" and "5" have been flushed.
                            assertThat(
                                            ((CronetBidirectionalStream) stream)
                                                    .getFlushDataForTesting())
                                    .isEmpty();
                        } else if (mNumWriteCompleted == 5) {
                            // Now flush "6", which is still in pending queue.
                            List<ByteBuffer> pendingData =
                                    ((CronetBidirectionalStream) stream).getPendingDataForTesting();
                            assertThat(pendingData).hasSize(1);
                            ByteBuffer pendingBuffer = pendingData.get(0);
                            byte[] content = new byte[pendingBuffer.remaining()];
                            pendingBuffer.get(content);
                            assertThat(content).isEqualTo("6".getBytes());

                            stream.flush();

                            assertThat(
                                            ((CronetBidirectionalStream) stream)
                                                    .getPendingDataForTesting())
                                    .isEmpty();
                            assertThat(
                                            ((CronetBidirectionalStream) stream)
                                                    .getFlushDataForTesting())
                                    .isEmpty();
                        }
                    }
                };
        callback.addWriteData("1".getBytes(), false);
        callback.addWriteData("2".getBytes(), false);
        callback.addWriteData("3".getBytes(), true);
        callback.addWriteData("4".getBytes(), false);
        callback.addWriteData("5".getBytes(), true);
        callback.addWriteData("6".getBytes(), false);
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream)
                        mCronetEngine
                                .newBidirectionalStreamBuilder(
                                        url, callback, callback.getExecutor())
                                .addHeader("foo", "bar")
                                .addHeader("empty", "")
                                .addHeader("Content-Type", "zebra")
                                .build();
        callback.setAutoAdvance(false);
        stream.start();
        waitOnStreamReady.block();

        assertThat(stream.getPendingDataForTesting()).isEmpty();
        assertThat(stream.getFlushDataForTesting()).isEmpty();

        // Write 1, 2, 3 and flush().
        callback.startNextWrite(stream);
        // Write 4, 5 and flush(). 4, 5 will be in flush queue.
        callback.startNextWrite(stream);
        // Write 6, but do not flush. 6 will be in pending queue.
        callback.startNextWrite(stream);

        waitForHeaders.block();
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);

        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("123456");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    // Regression test for crbug.com/692168.
    public void testCancelWhileWriteDataPending() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        // Use a direct executor to avoid race.
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback(/* useDirectExecutor= */ true) {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        // Start the first write.
                        stream.write(getSampleData(), false);
                        stream.flush();
                    }

                    @Override
                    public void onReadCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer byteBuffer,
                            boolean endOfStream) {
                        super.onReadCompleted(stream, info, byteBuffer, endOfStream);
                        // Cancel now when the write side is busy.
                        stream.cancel();
                    }

                    @Override
                    public void onWriteCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer buffer,
                            boolean endOfStream) {
                        // Flush twice to keep the flush queue non-empty.
                        stream.write(getSampleData(), false);
                        stream.flush();
                        stream.write(getSampleData(), false);
                        stream.flush();
                    }

                    // Returns a piece of sample data to send to the server.
                    private ByteBuffer getSampleData() {
                        byte[] data = new byte[100];
                        for (int i = 0; i < data.length; i++) {
                            data[i] = 'x';
                        }
                        ByteBuffer sampleData = ByteBuffer.allocateDirect(data.length);
                        sampleData.put(data);
                        sampleData.flip();
                        return sampleData;
                    }
                };
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(callback.mOnCanceledCalled).isTrue();
    }

    @Test
    @SmallTest
    public void testSimpleGetWithFlush() throws Exception {
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String url = Http2TestServer.getEchoStreamUrl();
            TestBidirectionalStreamCallback callback =
                    new TestBidirectionalStreamCallback() {
                        @Override
                        public void onStreamReady(BidirectionalStream stream) {
                            // Attempt to write data for GET request.
                            assertThrows(
                                    IllegalArgumentException.class,
                                    () -> stream.write(ByteBuffer.wrap("sample".getBytes()), true));

                            // If there are delayed headers, this flush should try to send them.
                            // If nothing to flush, it should not crash.
                            stream.flush();
                            super.onStreamReady(stream);

                            // Attempt to write data for GET request.
                            assertThrows(
                                    IllegalArgumentException.class,
                                    () -> stream.write(ByteBuffer.wrap("sample".getBytes()), true));
                        }
                    };
            BidirectionalStream stream =
                    mCronetEngine
                            .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                            .setHttpMethod("GET")
                            .delayRequestHeadersUntilFirstFlush(i == 0)
                            .addHeader("foo", "bar")
                            .addHeader("empty", "")
                            .build();
            // Flush before stream is started should not crash.
            stream.flush();

            stream.start();
            callback.blockForDone();
            assertThat(stream.isDone()).isTrue();

            // Flush after stream is completed is no-op. It shouldn't call into the destroyed
            // adapter.
            stream.flush();

            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            assertThat(callback.mResponseAsString).isEmpty();
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-empty", Arrays.asList(""));
        }
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlushAfterOneWrite() throws Exception {
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String url = Http2TestServer.getEchoStreamUrl();
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            callback.addWriteData("Test String".getBytes(), true);
            BidirectionalStream stream =
                    mCronetEngine
                            .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                            .delayRequestHeadersUntilFirstFlush(i == 0)
                            .addHeader("foo", "bar")
                            .addHeader("empty", "")
                            .addHeader("Content-Type", "zebra")
                            .build();
            stream.start();
            callback.blockForDone();
            assertThat(stream.isDone()).isTrue();

            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            assertThat(callback.mResponseAsString).isEqualTo("Test String");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-empty", Arrays.asList(""));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-content-type", Arrays.asList("zebra"));
        }
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlushTwice() throws Exception {
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String url = Http2TestServer.getEchoStreamUrl();
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            callback.addWriteData("Test String".getBytes(), false);
            callback.addWriteData("1234567890".getBytes(), false);
            callback.addWriteData("woot!".getBytes(), true);
            callback.addWriteData("Test String".getBytes(), false);
            callback.addWriteData("1234567890".getBytes(), false);
            callback.addWriteData("woot!".getBytes(), true);
            BidirectionalStream stream =
                    mCronetEngine
                            .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                            .delayRequestHeadersUntilFirstFlush(i == 0)
                            .addHeader("foo", "bar")
                            .addHeader("empty", "")
                            .addHeader("Content-Type", "zebra")
                            .build();
            stream.start();
            callback.blockForDone();
            assertThat(stream.isDone()).isTrue();
            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            assertThat(callback.mResponseAsString)
                    .isEqualTo("Test String1234567890woot!Test String1234567890woot!");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-empty", Arrays.asList(""));
            assertThat(callback.getResponseInfoWithChecks())
                    .hasHeadersThat()
                    .containsEntry("echo-content-type", Arrays.asList("zebra"));
        }
    }

    @Test
    @SmallTest
    // Tests that it is legal to call read() in onStreamReady().
    public void testReadDuringOnStreamReady() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        super.onStreamReady(stream);
                        startNextRead(stream);
                    }

                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        // Do nothing. Skip readng.
                    }
                };
        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    // Tests that it is legal to call flush() when previous nativeWritevData has
    // yet to complete.
    public void testSimplePostWithFlushBeforePreviousWriteCompleted() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        super.onStreamReady(stream);
                        // Write a second time before the previous nativeWritevData has completed.
                        startNextWrite(stream);
                        assertThat(numPendingWrites()).isEqualTo(0);
                    }
                };
        callback.addWriteData("Test String".getBytes(), false);
        callback.addWriteData("1234567890".getBytes(), false);
        callback.addWriteData("woot!".getBytes(), true);
        callback.addWriteData("Test String".getBytes(), false);
        callback.addWriteData("1234567890".getBytes(), false);
        callback.addWriteData("woot!".getBytes(), true);
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo("Test String1234567890woot!Test String1234567890woot!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    public void testSimplePut() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Put This Data!".getBytes());
        String methodName = "PUT";
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        builder.setHttpMethod(methodName);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Put This Data!");
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-method", Arrays.asList(methodName));
    }

    @Test
    @SmallTest
    public void testBadMethod() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        builder.setHttpMethod("bad:method!");
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.build().start());
        assertThat(e).hasMessageThat().isEqualTo("Invalid http method bad:method!");
    }

    @Test
    @SmallTest
    public void testBadHeaderName() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        builder.addHeader("goodheader1", "headervalue");
        builder.addHeader("header:name", "headervalue");
        builder.addHeader("goodheader2", "headervalue");
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.build().start());
        if (mTestRule.implementationUnderTest() == CronetImplementation.AOSP_PLATFORM
                && !mTestRule.isRunningInAOSP()) {
            // TODO(b/307234565): Remove check once chromium Android 14 emulator has latest changes.
            assertThat(e).hasMessageThat().isEqualTo("Invalid header header:name=headervalue");
        } else {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header with headername: header:name");
        }
    }

    @Test
    @SmallTest
    public void testBadHeaderValue() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        builder.addHeader("headername", "bad header\r\nvalue");
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.build().start());
        if (mTestRule.implementationUnderTest() == CronetImplementation.AOSP_PLATFORM
                && !mTestRule.isRunningInAOSP()) {
            // TODO(b/307234565): Remove check once chromium Android 14 emulator has latest changes.
            assertThat(e)
                    .hasMessageThat()
                    .isEqualTo("Invalid header headername=bad header\r\nvalue");
        } else {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header with headername: headername");
        }
    }

    @Test
    @SmallTest
    public void testAddHeader() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoHeaderUrl(headerName),
                        callback,
                        callback.getExecutor());
        builder.addHeader(headerName, headerValue);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(headerValue);
    }

    @Test
    @SmallTest
    public void testMultiRequestHeaders() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue1 = "header-value1";
        String headerValue2 = "header-value2";
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoAllHeadersUrl(), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue1);
        builder.addHeader(headerName, headerValue2);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String headers = callback.mResponseAsString;
        Pattern pattern = Pattern.compile(headerName + ":\\s(.*)\\r\\n");
        Matcher matcher = pattern.matcher(headers);
        List<String> actualValues = new ArrayList<String>();
        while (matcher.find()) {
            actualValues.add(matcher.group(1));
        }

        assertThat(actualValues).containsExactly("header-value2");
    }

    @Test
    @SmallTest
    public void testEchoTrailers() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoTrailersUrl(), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mTrailers).isNotNull();
        // Verify that header value is properly echoed in trailers.
        assertThat(callback.mTrailers.getAsMap())
                .containsEntry("echo-" + headerName, Arrays.asList(headerValue));
    }

    @Test
    @SmallTest
    public void testCustomUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoHeaderUrl(userAgentName),
                        callback,
                        callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.addHeader(userAgentName, userAgentValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    public void testCustomCronetEngineUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        ExperimentalCronetEngine.Builder engineBuilder =
                new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
        engineBuilder.setUserAgent(userAgentValue);
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            CronetTestUtil.setMockCertVerifierForTesting(
                    engineBuilder, QuicTestServer.createMockCertVerifier());
        }
        ExperimentalCronetEngine engine = engineBuilder.build();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                engine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoHeaderUrl(userAgentName),
                        callback,
                        callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    public void testDefaultUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoHeaderUrl(userAgentName),
                        callback,
                        callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(
                        new CronetEngine.Builder(mTestRule.getTestFramework().getContext())
                                .getDefaultUserAgent());
    }

    @Test
    @SmallTest
    public void testEchoStream() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String[] testData = {"Test String", createLongString("1234567890", 50000), "woot!"};
        StringBuilder stringData = new StringBuilder();
        for (String writeData : testData) {
            callback.addWriteData(writeData.getBytes());
            stringData.append(writeData);
        }
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "Value with Spaces")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-foo", Arrays.asList("Value with Spaces"));
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    public void testEchoStreamEmptyWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[0]);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEmpty();
    }

    @Test
    @SmallTest
    public void testDoubleWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        // super class will call Write() once.
                        super.onStreamReady(stream);
                        // Call Write() again.
                        startNextWrite(stream);
                        // Make sure there is no pending write.
                        assertThat(numPendingWrites()).isEqualTo(0);
                    }
                };
        callback.addWriteData("1".getBytes());
        callback.addWriteData("2".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void testDoubleRead() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        startNextRead(stream);
                        // Second read from callback invoked on single-threaded executor throws an
                        // exception because previous read is still pending until its completion is
                        // handled on executor.
                        Exception e =
                                assertThrows(
                                        Exception.class,
                                        () -> stream.read(ByteBuffer.allocateDirect(5)));
                        assertThat(e).hasMessageThat().isEqualTo("Unexpected read attempt.");
                    }
                };
        callback.addWriteData("1".getBytes());
        callback.addWriteData("2".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void testReadAndWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        // Start the write, that will not complete until callback completion.
                        setAutoAdvance(true);
                        startNextWrite(stream);
                        // Start the read. It is allowed with write in flight.
                        super.onResponseHeadersReceived(stream, info);
                    }
                };
        callback.setAutoAdvance(false);
        callback.addWriteData("1".getBytes());
        callback.addWriteData("2".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.waitForNextWriteStep();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    public void testEchoStreamWriteFirst() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        String[] testData = {"a", "bb", "ccc", "Test String", "1234567890", "woot!"};
        StringBuilder stringData = new StringBuilder();
        for (String writeData : testData) {
            callback.addWriteData(writeData.getBytes());
            stringData.append(writeData);
        }
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        // Write first.
        callback.waitForNextWriteStep(); // onStreamReady
        for (int i = 0; i < testData.length; i++) {
            // Write next chunk of test data.
            callback.startNextWrite(stream);
            callback.waitForNextWriteStep(); // onWriteCompleted
        }

        // Wait for read step, but don't read yet.
        callback.waitForNextReadStep(); // onResponseHeadersReceived
        assertThat(callback.mResponseAsString).isEmpty();
        // Read back.
        callback.startNextRead(stream);
        callback.waitForNextReadStep(); // onReadCompleted
        // Verify that some part of proper response is read.
        assertThat(callback.mResponseAsString).startsWith(testData[0]);
        // Read the rest of the response.
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
    }

    @Test
    @SmallTest
    public void testEchoStreamStepByStep() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        String[] testData = {"a", "bb", "ccc", "Test String", "1234567890", "woot!"};
        StringBuilder stringData = new StringBuilder();
        for (String writeData : testData) {
            callback.addWriteData(writeData.getBytes());
            stringData.append(writeData);
        }
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.waitForNextWriteStep();
        callback.waitForNextReadStep();

        for (String expected : testData) {
            // Write next chunk of test data.
            callback.startNextWrite(stream);
            callback.waitForNextWriteStep();

            // Read next chunk of test data.
            ByteBuffer readBuffer = ByteBuffer.allocateDirect(100);
            callback.startNextRead(stream, readBuffer);
            callback.waitForNextReadStep();
            assertThat(readBuffer.position()).isEqualTo(expected.length());
            assertThat(stream.isDone()).isFalse();
        }

        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
    }

    /** Checks that the buffer is updated correctly, when starting at an offset. */
    @Test
    @SmallTest
    public void testSimpleGetBufferUpdates() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        // Since the method is "GET", the expected response body is also "GET".
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();

        assertThat(callback.mError).isNull();
        assertThat(callback.isDone()).isFalse();
        assertThat(callback.mResponseStep)
                .isEqualTo(TestBidirectionalStreamCallback.ResponseStep.ON_RESPONSE_STARTED);

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertThat(readBuffer.position()).isEqualTo(3);

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (callback.mResponseAsString.length() < 2) {
            assertThat(callback.isDone()).isFalse();
            callback.startNextRead(stream, readBuffer);
            callback.waitForNextReadStep();
        }

        // Make sure the two characters were read.
        assertThat(callback.mResponseAsString).isEqualTo("GE");

        // Check the contents of the entire buffer. The first 3 characters
        // should not have been changed, and the last two should be the first
        // two characters from the response.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORGE");
        // The limit and position should be 5.
        assertThat(readBuffer.limit()).isEqualTo(5);
        assertThat(readBuffer.position()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);

        // Start reading from position 3. Since the only remaining character
        // from the response is a "T", when the read completes, the buffer
        // should contain "FORTE", with a position() of 4 and a limit() of 5.
        readBuffer.position(3);
        callback.startNextRead(stream, readBuffer);
        callback.waitForNextReadStep();

        // Make sure all three characters of the response have now been read.
        assertThat(callback.mResponseAsString).isEqualTo("GET");

        // Check the entire contents of the buffer. Only the third character
        // should have been modified.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORTE");

        // Make sure position and limit were updated correctly.
        assertThat(readBuffer.position()).isEqualTo(4);
        assertThat(readBuffer.limit()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);

        // One more read attempt. The request should complete.
        readBuffer.position(1);
        readBuffer.limit(5);
        callback.setAutoAdvance(true);
        callback.startNextRead(stream, readBuffer);
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        checkResponseInfo(
                callback.getResponseInfoWithChecks(), Http2TestServer.getEchoMethodUrl(), 200, "");

        // Check that buffer contents were not modified.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORTE");

        // Position should not have been modified, since nothing was read.
        assertThat(readBuffer.position()).isEqualTo(1);
        // Limit should be unchanged as always.
        assertThat(readBuffer.limit()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);

        // TestRequestFinishedListener expects a single call to onRequestFinished. Here we
        // explicitly wait for the call to happen to avoid a race condition with the other
        // TestRequestFinishedListener created within runGetWithExpectedReceivedByteCount.
        requestFinishedListener.blockUntilDone();
        mCronetEngine.removeRequestFinishedListener(requestFinishedListener);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestBidirectionalCallback.
        // The expected received bytes count is lower than it would be for the first request on the
        // connection, because the server includes an HPACK dynamic table size update only in the
        // first response HEADERS frame.
        runGetWithExpectedReceivedByteCount(27);
    }

    @Test
    @SmallTest
    public void testBadBuffers() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();

        assertThat(callback.mError).isNull();
        assertThat(callback.isDone()).isFalse();
        assertThat(callback.mResponseStep)
                .isEqualTo(TestBidirectionalStreamCallback.ResponseStep.ON_RESPONSE_STARTED);

        // Try to read using a full buffer.
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(4);
        readBuffer.put("full".getBytes());
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> stream.read(readBuffer));
        assertThat(e).hasMessageThat().isEqualTo("ByteBuffer is already full.");

        // Try to read using a non-direct buffer.
        ByteBuffer readBuffer1 = ByteBuffer.allocate(5);
        e = assertThrows(IllegalArgumentException.class, () -> stream.read(readBuffer1));
        assertThat(e).hasMessageThat().isEqualTo("byteBuffer must be a direct ByteBuffer.");

        // Finish the stream with a direct ByteBuffer.
        callback.setAutoAdvance(true);
        ByteBuffer readBuffer2 = ByteBuffer.allocateDirect(5);
        stream.read(readBuffer2);
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
    }

    private void throwOrCancel(
            FailureType failureType, ResponseStep failureStep, boolean expectError) {
        // Use a fresh CronetEngine each time so Http2 session is not reused.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        }
        mCronetEngine = builder.build();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setFailure(failureType, failureStep);
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        BidirectionalStream.Builder streamBuilder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = streamBuilder.setHttpMethod("GET").build();
        Date startTime = new Date();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        RequestFinishedInfo.Metrics metrics = finishedInfo.getMetrics();
        assertThat(metrics).isNotNull();
        // Cancellation when stream is ready does not guarantee that
        // mResponseInfo is null because there might be a
        // onResponseHeadersReceived already queued in the executor.
        // See crbug.com/594432.
        if (failureStep != ResponseStep.ON_STREAM_READY) {
            assertThat(callback.getResponseInfo()).isNotNull();
        }
        CronetImplementation implementationUnderTest = mTestRule.implementationUnderTest();
        // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
        // metrics. Don't bother checking timing metrics for AOSP whether it passes or not.
        if (implementationUnderTest != CronetImplementation.AOSP_PLATFORM) {
            // Check metrics information.
            if (failureStep == ResponseStep.ON_RESPONSE_STARTED
                    || failureStep == ResponseStep.ON_READ_COMPLETED
                    || failureStep == ResponseStep.ON_TRAILERS) {
                // For steps after response headers are received, there will be
                // connect timing metrics.
                MetricsTestUtil.checkTimingMetrics(
                        implementationUnderTest, metrics, startTime, endTime);
                MetricsTestUtil.checkHasConnectTiming(
                        implementationUnderTest, metrics, startTime, endTime, true);
                assertThat(metrics.getSentByteCount()).isGreaterThan(0L);
                assertThat(metrics.getReceivedByteCount()).isGreaterThan(0L);
            } else if (failureStep == ResponseStep.ON_STREAM_READY) {
                assertThat(metrics.getRequestStart()).isNotNull();
                MetricsTestUtil.assertAfter(metrics.getRequestStart(), startTime);
                assertThat(metrics.getRequestEnd()).isNotNull();
                MetricsTestUtil.assertAfter(endTime, metrics.getRequestEnd());
                MetricsTestUtil.assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
            }
        }
        assertThat(callback.mError != null).isEqualTo(expectError);
        assertThat(callback.mOnErrorCalled).isEqualTo(expectError);
        if (expectError) {
            assertThat(finishedInfo.getException()).isNotNull();
            assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.FAILED);
        } else {
            assertThat(finishedInfo.getException()).isNull();
            assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.CANCELED);
        }
        assertThat(callback.mOnCanceledCalled)
                .isEqualTo(
                        failureType == FailureType.CANCEL_SYNC
                                || failureType == FailureType.CANCEL_ASYNC
                                || failureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE);
        mCronetEngine.removeRequestFinishedListener(requestFinishedListener);
    }

    @Test
    @SmallTest
    public void testFailures() throws Exception {
        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_STREAM_READY, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_STREAM_READY, false);
        throwOrCancel(FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_STREAM_READY, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_STREAM_READY, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_RESPONSE_STARTED, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_RESPONSE_STARTED, false);
        throwOrCancel(
                FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_RESPONSE_STARTED, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_RESPONSE_STARTED, true);

        throwOrCancel(FailureType.CANCEL_SYNC, ResponseStep.ON_READ_COMPLETED, false);
        throwOrCancel(FailureType.CANCEL_ASYNC, ResponseStep.ON_READ_COMPLETED, false);
        throwOrCancel(
                FailureType.CANCEL_ASYNC_WITHOUT_PAUSE, ResponseStep.ON_READ_COMPLETED, false);
        throwOrCancel(FailureType.THROW_SYNC, ResponseStep.ON_READ_COMPLETED, true);
    }

    @Test
    @SmallTest
    public void testCancelBeforeResponse() {
        // Use a hanging endpoint to prevent race between getting a response and cancel().
        // Cronet only records the responseInfo once onResponseHeadersReceived is called.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newBidirectionalStreamBuilder(
                                Http2TestServer.getHangingRequestUrl(),
                                callback,
                                callback.getExecutor());
        BidirectionalStream stream = builder.build();
        stream.start();
        stream.cancel();
        callback.blockForDone();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_CANCELED);
        assertThat(callback.getResponseInfo()).isNull();
    }

    @Test
    @SmallTest
    public void testThrowOnSucceeded() {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_SUCCEEDED);
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.blockForDone();
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).isNotNull();
        // Check that error thrown from 'onSucceeded' callback is not reported.
        assertThat(callback.mError).isNull();
        assertThat(callback.mOnErrorCalled).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "crbug.com/1494845: Requires access to internals not available in AOSP")
    public void testExecutorShutdownBeforeStreamIsDone() {
        // Test that stream is destroyed even if executor is shut down and rejects posting tasks.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();
        assertThat(callback.isDone()).isFalse();
        assertThat(stream.isDone()).isFalse();

        final ConditionVariable streamDestroyed = new ConditionVariable(false);
        stream.setOnDestroyedCallbackForTesting(
                new Runnable() {
                    @Override
                    public void run() {
                        streamDestroyed.open();
                    }
                });

        // Shut down the executor, so posting the task will throw an exception.
        callback.shutdownExecutor();
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        stream.read(readBuffer);
        // Callback will never be called again because executor is shut down,
        // but stream will be destroyed from network thread.
        streamDestroyed.block();

        assertThat(callback.isDone()).isFalse();
        assertThat(stream.isDone()).isTrue();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testCronetEngineShutdown() throws Exception {
        // Test that CronetEngine cannot be shut down if there are any active streams.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Block callback when response starts to verify that shutdown fails
        // if there are active streams.
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        Exception e = assertThrows(Exception.class, mCronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        e = assertThrows(Exception.class, mCronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");
        callback.startNextRead(stream);

        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);
        e = assertThrows(Exception.class, mCronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        // May not have read all the data, in theory. Just enable auto-advance
        // and finish the request.
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        waitForActiveRequestCount(0);
        mCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    @RequiresRestart("crbug.com/344665939")
    public void testCronetEngineShutdownAfterStreamFailure() throws Exception {
        // Test that CronetEngine can be shut down after stream reports a failure.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_READ_COMPLETED);
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        waitForActiveRequestCount(0);
        mCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testCronetEngineShutdownAfterStreamCancel() throws Exception {
        // Test that CronetEngine can be shut down after stream is canceled.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(
                        Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();

        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        stream.start();
        Exception e = assertThrows(Exception.class, mCronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");
        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        stream.cancel();
        callback.blockForDone();
        assertThat(callback.mOnCanceledCalled).isTrue();
        waitForActiveRequestCount(0);
        mCronetEngine.shutdown();
    }

    /*
     * Verifies NetworkException constructed from specific error codes are retryable.
     */
    @SmallTest
    @Test
    public void testErrorCodes() throws Exception {
        // Non-BidirectionalStream specific error codes.
        checkSpecificErrorCode(
                NetError.ERR_NAME_NOT_RESOLVED,
                NetworkException.ERROR_HOSTNAME_NOT_RESOLVED,
                false);
        checkSpecificErrorCode(
                NetError.ERR_INTERNET_DISCONNECTED,
                NetworkException.ERROR_INTERNET_DISCONNECTED,
                false);
        checkSpecificErrorCode(
                NetError.ERR_NETWORK_CHANGED, NetworkException.ERROR_NETWORK_CHANGED, true);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_CLOSED, NetworkException.ERROR_CONNECTION_CLOSED, true);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_REFUSED, NetworkException.ERROR_CONNECTION_REFUSED, false);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_RESET, NetworkException.ERROR_CONNECTION_RESET, true);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_TIMED_OUT,
                NetworkException.ERROR_CONNECTION_TIMED_OUT,
                true);
        checkSpecificErrorCode(NetError.ERR_TIMED_OUT, NetworkException.ERROR_TIMED_OUT, true);
        checkSpecificErrorCode(
                NetError.ERR_ADDRESS_UNREACHABLE,
                NetworkException.ERROR_ADDRESS_UNREACHABLE,
                false);
        // BidirectionalStream specific retryable error codes.
        checkSpecificErrorCode(NetError.ERR_HTTP2_PING_FAILED, NetworkException.ERROR_OTHER, true);
        checkSpecificErrorCode(
                NetError.ERR_QUIC_HANDSHAKE_FAILED, NetworkException.ERROR_OTHER, true);
    }

    // Returns the contents of byteBuffer, from its position() to its limit(),
    // as a String. Does not modify byteBuffer's position().
    private static String bufferContentsToString(ByteBuffer byteBuffer, int start, int end) {
        // Use a duplicate to avoid modifying byteBuffer.
        ByteBuffer duplicate = byteBuffer.duplicate();
        duplicate.position(start);
        duplicate.limit(end);
        byte[] contents = new byte[duplicate.remaining()];
        duplicate.get(contents);
        return new String(contents);
    }

    private static void checkSpecificErrorCode(
            int netError, int errorCode, boolean immediatelyRetryable) throws Exception {
        NetworkException exception =
                new BidirectionalStreamNetworkException("", errorCode, netError);
        assertThat(exception.immediatelyRetryable()).isEqualTo(immediatelyRetryable);
        assertThat(exception.getCronetInternalErrorCode()).isEqualTo(netError);
        assertThat(exception.getErrorCode()).isEqualTo(errorCode);
    }

    @Test
    @SmallTest
    @RequiresMinApi(10) // Tagging support added in API level 10: crrev.com/c/chromium/src/+/937583
    public void testTagging() throws Exception {
        if (!CronetTestUtil.nativeCanGetTaggedBytes()) {
            Log.i(TAG, "Skipping test - GetTaggedBytes unsupported.");
            return;
        }
        String url = Http2TestServer.getEchoStreamUrl();

        // Test untagged requests are given tag 0.
        int tag = 0;
        long priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[] {0});
        mCronetEngine
                .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                .build()
                .start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test explicit tagging.
        tag = 0x12345678;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[] {0});
        ExperimentalBidirectionalStream.Builder builder =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsTag(tag));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test a different tag value to make sure reused connections are retagged.
        tag = 0x87654321;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[] {0});
        builder =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsTag(tag));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);

        // Test tagging with our UID.
        tag = 0;
        priorBytes = CronetTestUtil.nativeGetTaggedBytes(tag);
        callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[] {0});
        builder =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor());
        assertThat(builder).isEqualTo(builder.setTrafficStatsUid(Process.myUid()));
        builder.build().start();
        callback.blockForDone();
        assertThat(CronetTestUtil.nativeGetTaggedBytes(tag)).isGreaterThan(priorBytes);
    }

    @Test
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "b/309112420 BidiStream bindToNetwork API not exposed in AOSP")
    public void testBindToInvalidNetworkFails() {
        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();

        BidirectionalStream.Builder builder =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET");

        if (mTestRule.implementationUnderTest() == CronetImplementation.AOSP_PLATFORM) {
            // android.net.http.UrlRequestBuilder#bindToNetwork requires an android.net.Network
            // object. So, in this case, it will be the wrapper layer that will fail to translate
            // that to a Network, not something in net's code. Hence, the failure will manifest
            // itself at bind time, not at request execution time.
            // Note: this will never happen in prod, as translation failure can only happen if we're
            // given a fake networkHandle.
            assertThrows(
                    IllegalArgumentException.class,
                    () -> builder.bindToNetwork(-150 /* invalid network handle */).build());
            return;
        }

        builder.bindToNetwork(-150 /* invalid network handle */);
        BidirectionalStream stream = builder.build();
        stream.start();

        callback.blockForDone();

        assertThat(callback.mError).isNotNull();
        if (mTestRule.implementationUnderTest() == CronetImplementation.FALLBACK) {
            assertThat(callback.mError).isInstanceOf(CronetExceptionImpl.class);
            assertThat(callback.mError).hasCauseThat().isInstanceOf(NetworkExceptionImpl.class);
        } else {
            assertThat(callback.mError).isInstanceOf(NetworkExceptionImpl.class);
        }
    }

    @Test
    // TODO(crbug.com/41494733): Enable on Android M once fixed.
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void testBindToDefaultNetworkSucceeds() {
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = delegate.getDefaultNetwork();
        assume().that(defaultNetwork).isNotNull();

        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();

        BidirectionalStream.Builder builder =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET");

        builder.bindToNetwork(defaultNetwork.getNetworkHandle());
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onStreamReady_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        callbackStream.set(stream);
                        super.onStreamReady(stream);
                    }
                };

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onReadCompleted_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onReadCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer byteBuffer,
                            boolean endOfStream) {
                        callbackStream.set(stream);
                        super.onReadCompleted(stream, info, byteBuffer, endOfStream);
                    }
                };

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onWriteCompleted_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onWriteCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer byteBuffer,
                            boolean endOfStream) {
                        callbackStream.set(stream);
                        super.onWriteCompleted(stream, info, byteBuffer, endOfStream);
                    }
                };
        callback.addWriteData("1".getBytes());

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "POST");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onResponseHeaders_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        callbackStream.set(stream);
                        super.onResponseHeadersReceived(stream, info);
                    }
                };

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onResponseTrailersReceived_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onResponseTrailersReceived(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            UrlResponseInfo.HeaderBlock trailers) {
                        callbackStream.set(stream);
                        super.onResponseTrailersReceived(stream, info, trailers);
                    }
                };

        startStreamAndAssertCallback(
                Http2TestServer.getEchoTrailersUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onSucceeded_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
                        callbackStream.set(stream);
                        super.onSucceeded(stream, info);
                    }
                };

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onFailed_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onFailed(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            CronetException error) {
                        callbackStream.set(stream);
                        super.onFailed(stream, info, error);
                    }
                };
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_STREAM_READY);

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallbackMethod_onCanceled_receivesSameStreamObject() {
        AtomicReference<BidirectionalStream> callbackStream = new AtomicReference<>();
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {

                    @Override
                    public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
                        callbackStream.set(stream);
                        super.onCanceled(stream, info);
                    }
                };
        callback.setFailure(FailureType.CANCEL_SYNC, ResponseStep.ON_STREAM_READY);

        startStreamAndAssertCallback(
                Http2TestServer.getServerUrl(), callback, callbackStream, "GET");
    }

    private void startStreamAndAssertCallback(
            String url,
            TestBidirectionalStreamCallback callback,
            AtomicReference<BidirectionalStream> callbackStream,
            String method) {
        BidirectionalStream stream =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod(method)
                        .build();
        stream.start();
        callback.blockForDone();

        assertThat(callbackStream.get() == stream).isTrue();
    }

    // While our documentation does not specify that the stream passed to the callbacks is the same
    // object, it is an implicit expectation by our users that we should not break.
    // See b/328442628 for an example regression.
    @Test
    public void testCallback_twoStreamsFromOneBuilder_receivesCorrectStreamObject() {
        AtomicReference<BidirectionalStream> onStreamReadyStream = new AtomicReference<>();
        AtomicReference<BidirectionalStream> onResponseHeadersStream = new AtomicReference<>();
        AtomicReference<BidirectionalStream> onReadCompletedStream = new AtomicReference<>();
        AtomicReference<BidirectionalStream> onSucceededStream = new AtomicReference<>();
        TestBidirectionalStreamCallback.SimpleSucceedingCallback callback =
                new TestBidirectionalStreamCallback.SimpleSucceedingCallback() {
                    @Override
                    public void onStreamReady(BidirectionalStream stream) {
                        onStreamReadyStream.set(stream);
                        super.onStreamReady(stream);
                    }

                    @Override
                    public void onResponseHeadersReceived(
                            BidirectionalStream stream, UrlResponseInfo info) {
                        onResponseHeadersStream.set(stream);
                        super.onResponseHeadersReceived(stream, info);
                    }

                    @Override
                    public void onReadCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer byteBuffer,
                            boolean endOfStream) {
                        onReadCompletedStream.set(stream);
                        super.onReadCompleted(stream, info, byteBuffer, endOfStream);
                    }

                    @Override
                    public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
                        onSucceededStream.set(stream);
                        super.onSucceeded(stream, info);
                    }
                };

        BidirectionalStream.Builder builder =
                mTestRule
                        .getTestFramework()
                        .getEngine()
                        .newBidirectionalStreamBuilder(
                                Http2TestServer.getServerUrl(), callback, callback.getExecutor())
                        .setHttpMethod("GET");
        BidirectionalStream stream1 = builder.build();
        BidirectionalStream stream2 = builder.build();
        stream1.start();
        callback.done.block();

        assertThat(onStreamReadyStream.get() == stream1).isTrue();
        assertThat(onResponseHeadersStream.get() == stream1).isTrue();
        assertThat(onReadCompletedStream.get() == stream1).isTrue();
        assertThat(onSucceededStream.get() == stream1).isTrue();

        callback.done.close();
        stream2.start();
        callback.done.block();

        assertThat(onStreamReadyStream.get() == stream2).isTrue();
        assertThat(onResponseHeadersStream.get() == stream2).isTrue();
        assertThat(onReadCompletedStream.get() == stream2).isTrue();
        assertThat(onSucceededStream.get() == stream2).isTrue();
    }

    /**
     * Cronet does not currently provide an API to wait for the active request count to change. We
     * can't just wait for the terminal callback to fire because Cronet updates the count some time
     * *after* we return from the callback. We hack around this by polling the active request count
     * in a loop.
     */
    private void waitForActiveRequestCount(int expectedCount) throws Exception {
        while (mCronetEngine.getActiveRequestCount() != expectedCount) Thread.sleep(100);
    }
}
