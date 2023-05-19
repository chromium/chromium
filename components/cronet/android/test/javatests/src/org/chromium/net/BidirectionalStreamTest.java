// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.net.CronetTestRule.assertContains;
import static org.chromium.net.CronetTestRule.getContext;

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
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.TestBidirectionalStreamCallback.FailureType;
import org.chromium.net.TestBidirectionalStreamCallback.ResponseStep;
import org.chromium.net.impl.BidirectionalStreamNetworkException;
import org.chromium.net.impl.CronetBidirectionalStream;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test functionality of BidirectionalStream interface.
 */
@RunWith(AndroidJUnit4.class)
public class BidirectionalStreamTest {
    private static final String TAG = BidirectionalStreamTest.class.getSimpleName();

    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private ExperimentalCronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());

        mCronetEngine = builder.build();
        assertTrue(Http2TestServer.startHttp2TestServer(getContext()));
    }

    @After
    public void tearDown() throws Exception {
        assertTrue(Http2TestServer.shutdownHttp2TestServer());
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
        }
    }

    private static void checkResponseInfo(UrlResponseInfo responseInfo, String expectedUrl,
            int expectedHttpStatusCode, String expectedHttpStatusText) {
        assertThat(responseInfo.getUrl()).isEqualTo(expectedUrl);
        assertThat(responseInfo.getUrlChain()).containsExactly(expectedUrl);
        assertThat(responseInfo.getHttpStatusCode()).isEqualTo(expectedHttpStatusCode);
        assertThat(responseInfo.getHttpStatusText()).isEqualTo(expectedHttpStatusText);
        assertFalse(responseInfo.wasCached());
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
            headersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                    headers[i], headers[i + 1]));
        }
        UrlResponseInfoImpl urlResponseInfo = new UrlResponseInfoImpl(Arrays.asList(urls),
                statusCode, message, headersList, false, "h2", null, receivedBytes);
        return urlResponseInfo;
    }

    private void runSimpleGetWithExpectedReceivedByteCount(int expectedReceivedBytes)
            throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        requestFinishedListener.blockUntilDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        UrlResponseInfo urlResponseInfo = createUrlResponseInfo(
                new String[] {url}, "", 200, expectedReceivedBytes, ":status", "200");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.mResponseInfo);
        checkResponseInfo(callback.mResponseInfo, Http2TestServer.getEchoMethodUrl(), 200, "");
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        assertTrue(finishedInfo.getAnnotations().isEmpty());
    }

    @Test
    @SmallTest
    public void testBuilderCheck() throws Exception {
        if (mTestRule.testingJavaImpl()) {
            runBuilderCheckJavaImpl();
        } else {
            runBuilderCheckNativeImpl();
        }
    }

    private void runBuilderCheckNativeImpl() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        try {
            mCronetEngine.newBidirectionalStreamBuilder(null, callback, callback.getExecutor());
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("URL is required.");
        }
        try {
            mCronetEngine.newBidirectionalStreamBuilder(
                    Http2TestServer.getServerUrl(), null, callback.getExecutor());
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Callback is required.");
        }
        try {
            mCronetEngine.newBidirectionalStreamBuilder(
                    Http2TestServer.getServerUrl(), callback, null);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Executor is required.");
        }
        // Verify successful creation doesn't throw.
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        try {
            builder.addHeader(null, "value");
            fail("Header name is not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header name.");
        }
        try {
            builder.addHeader("name", null);
            fail("Header value is not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header value.");
        }
        try {
            builder.setHttpMethod(null);
            fail("Method name is not null-checked");
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("Method is required.");
        }
    }

    private void runBuilderCheckJavaImpl() {
        try {
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            CronetTestRule.createJavaEngineBuilder(CronetTestRule.getContext())
                    .build()
                    .newBidirectionalStreamBuilder(
                            Http2TestServer.getServerUrl(), callback, callback.getExecutor());
            fail("JavaCronetEngine doesn't support BidirectionalStream."
                    + " Expected UnsupportedOperationException");
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testFailPlainHttp() throws Exception {
        String url = "http://example.com";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertContains("Exception in BidirectionalStream: net::ERR_DISALLOWED_URL_SCHEME",
                callback.mError.getMessage());
        assertThat(((NetworkException) callback.mError).getCronetInternalErrorCode())
                .isEqualTo(-301);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimpleGet() throws Exception {
        // Since this is the first request on the connection, the expected received bytes count
        // must account for an HPACK dynamic table size update.
        runSimpleGetWithExpectedReceivedByteCount(31);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimpleHead() throws Exception {
        String url = Http2TestServer.getEchoMethodUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("HEAD")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("HEAD");
        UrlResponseInfo urlResponseInfo =
                createUrlResponseInfo(new String[] {url}, "", 200, 32, ":status", "200");
        mTestRule.assertResponseEquals(urlResponseInfo, callback.mResponseInfo);
        checkResponseInfo(callback.mResponseInfo, Http2TestServer.getEchoMethodUrl(), 200, "");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimplePost() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .addRequestAnnotation(this)
                        .addRequestAnnotation("request annotation")
                        .build();
        Date startTime = new Date();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(finishedInfo, url, startTime, endTime);
        assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(finishedInfo.getMetrics(), startTime, endTime, true);
        assertThat(finishedInfo.getAnnotations()).containsExactly("request annotation", this);
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testGetActiveRequestCount() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        callback.setBlockOnTerminalState(true);
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(Http2TestServer.getEchoStreamUrl(), callback,
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
    @OnlyRunNativeCronet
    public void testGetActiveRequestCountWithInvalidRequest() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(Http2TestServer.getEchoStreamUrl(), callback,
                                callback.getExecutor())
                        .addHeader("", "") // Deliberately invalid
                        .build();
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(0);
        assertThrows(IllegalArgumentException.class, stream::start);
        assertThat(mCronetEngine.getActiveRequestCount()).isEqualTo(0);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimpleGetWithCombinedHeader() throws Exception {
        String url = Http2TestServer.getCombinedHeadersUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        requestFinishedListener.blockUntilDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        // Default method is 'GET'.
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("foo", Arrays.asList("bar", "bar2"));
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        assertTrue(finishedInfo.getAnnotations().isEmpty());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimplePostWithFlush() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes(), false);
        callback.addWriteData("1234567890".getBytes(), false);
        callback.addWriteData("woot!".getBytes(), true);
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        // Flush before stream is started should not crash.
        stream.flush();

        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());

        // Flush after stream is completed is no-op. It shouldn't call into the destroyed adapter.
        stream.flush();

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Tests that a delayed flush() only sends buffers that have been written
    // before it is called, and it doesn't flush buffers in mPendingQueue.
    public void testFlushData() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        final ConditionVariable waitOnStreamReady = new ConditionVariable();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
            // Number of onWriteCompleted callbacks that have been invoked.
            private int mNumWriteCompleted;

            @Override
            public void onStreamReady(BidirectionalStream stream) {
                mResponseStep = ResponseStep.ON_STREAM_READY;
                waitOnStreamReady.open();
            }

            @Override
            public void onWriteCompleted(BidirectionalStream stream, UrlResponseInfo info,
                    ByteBuffer buffer, boolean endOfStream) {
                super.onWriteCompleted(stream, info, buffer, endOfStream);
                mNumWriteCompleted++;
                if (mNumWriteCompleted <= 3) {
                    // "6" is in pending queue.
                    List<ByteBuffer> pendingData =
                            ((CronetBidirectionalStream) stream).getPendingDataForTesting();
                    assertThat(pendingData).hasSize(1);
                    ByteBuffer pendingBuffer = pendingData.get(0);
                    byte[] content = new byte[pendingBuffer.remaining()];
                    pendingBuffer.get(content);
                    assertTrue(Arrays.equals("6".getBytes(), content));

                    // "4" and "5" have been flushed.
                    assertThat(((CronetBidirectionalStream) stream).getFlushDataForTesting())
                            .isEmpty();
                } else if (mNumWriteCompleted == 5) {
                    // Now flush "6", which is still in pending queue.
                    List<ByteBuffer> pendingData =
                            ((CronetBidirectionalStream) stream).getPendingDataForTesting();
                    assertThat(pendingData).hasSize(1);
                    ByteBuffer pendingBuffer = pendingData.get(0);
                    byte[] content = new byte[pendingBuffer.remaining()];
                    pendingBuffer.get(content);
                    assertTrue(Arrays.equals("6".getBytes(), content));

                    stream.flush();

                    assertThat(((CronetBidirectionalStream) stream).getPendingDataForTesting())
                            .isEmpty();
                    assertThat(((CronetBidirectionalStream) stream).getFlushDataForTesting())
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
                (CronetBidirectionalStream) mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
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

        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("123456");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Regression test for crbug.com/692168.
    public void testCancelWhileWriteDataPending() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        // Use a direct executor to avoid race.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback(
                /*useDirectExecutor*/ true) {
            @Override
            public void onStreamReady(BidirectionalStream stream) {
                // Start the first write.
                stream.write(getDummyData(), false);
                stream.flush();
            }
            @Override
            public void onReadCompleted(BidirectionalStream stream, UrlResponseInfo info,
                    ByteBuffer byteBuffer, boolean endOfStream) {
                super.onReadCompleted(stream, info, byteBuffer, endOfStream);
                // Cancel now when the write side is busy.
                stream.cancel();
            }
            @Override
            public void onWriteCompleted(BidirectionalStream stream, UrlResponseInfo info,
                    ByteBuffer buffer, boolean endOfStream) {
                // Flush twice to keep the flush queue non-empty.
                stream.write(getDummyData(), false);
                stream.flush();
                stream.write(getDummyData(), false);
                stream.flush();
            }
            // Returns a piece of dummy data to send to the server.
            private ByteBuffer getDummyData() {
                byte[] data = new byte[100];
                for (int i = 0; i < data.length; i++) {
                    data[i] = 'x';
                }
                ByteBuffer dummyData = ByteBuffer.allocateDirect(data.length);
                dummyData.put(data);
                dummyData.flip();
                return dummyData;
            }
        };
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(callback.mOnCanceledCalled);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimpleGetWithFlush() throws Exception {
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String url = Http2TestServer.getEchoStreamUrl();
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
                @Override
                public void onStreamReady(BidirectionalStream stream) {
                    try {
                        // Attempt to write data for GET request.
                        stream.write(ByteBuffer.wrap("dummy".getBytes()), true);
                    } catch (IllegalArgumentException e) {
                        // Expected.
                    }
                    // If there are delayed headers, this flush should try to send them.
                    // If nothing to flush, it should not crash.
                    stream.flush();
                    super.onStreamReady(stream);
                    try {
                        // Attempt to write data for GET request.
                        stream.write(ByteBuffer.wrap("dummy".getBytes()), true);
                    } catch (IllegalArgumentException e) {
                        // Expected.
                    }
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
            assertTrue(stream.isDone());

            // Flush after stream is completed is no-op. It shouldn't call into the destroyed
            // adapter.
            stream.flush();

            assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
            assertThat(callback.mResponseAsString).isEmpty();
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-empty", Arrays.asList(""));
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
            assertTrue(stream.isDone());

            assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
            assertThat(callback.mResponseAsString).isEqualTo("Test String");
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-empty", Arrays.asList(""));
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-content-type", Arrays.asList("zebra"));
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
            assertTrue(stream.isDone());
            assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
            assertThat(callback.mResponseAsString)
                    .isEqualTo("Test String1234567890woot!Test String1234567890woot!");
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-foo", Arrays.asList("bar"));
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-empty", Arrays.asList(""));
            assertThat(callback.mResponseInfo.getAllHeaders())
                    .containsEntry("echo-content-type", Arrays.asList("zebra"));
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Tests that it is legal to call read() in onStreamReady().
    public void testReadDuringOnStreamReady() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String1234567890woot!");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    // Tests that it is legal to call flush() when previous nativeWritevData has
    // yet to complete.
    public void testSimplePostWithFlushBeforePreviousWriteCompleted() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo("Test String1234567890woot!Test String1234567890woot!");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("bar"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-empty", Arrays.asList(""));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimplePut() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Put This Data!".getBytes());
        String methodName = "PUT";
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        builder.setHttpMethod(methodName);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Put This Data!");
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-method", Arrays.asList(methodName));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBadMethod() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        try {
            builder.setHttpMethod("bad:method!");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid http method bad:method!");
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBadHeaderName() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        try {
            builder.addHeader("goodheader1", "headervalue");
            builder.addHeader("header:name", "headervalue");
            builder.addHeader("goodheader2", "headervalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo("Invalid header header:name=headervalue");
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBadHeaderValue() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getServerUrl(), callback, callback.getExecutor());
        try {
            builder.addHeader("headername", "bad header\r\nvalue");
            builder.build().start();
            fail("IllegalArgumentException not thrown.");
        } catch (IllegalArgumentException e) {
            assertThat(e).hasMessageThat().isEqualTo(
                    "Invalid header headername=bad header\r\nvalue");
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testAddHeader() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoHeaderUrl(headerName), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(headerValue);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testMultiRequestHeaders() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue1 = "header-value1";
        String headerValue2 = "header-value2";
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoAllHeadersUrl(), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue1);
        builder.addHeader(headerName, headerValue2);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
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
    @OnlyRunNativeCronet
    public void testEchoTrailers() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        String headerName = "header-name";
        String headerValue = "header-value";
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoTrailersUrl(), callback, callback.getExecutor());
        builder.addHeader(headerName, headerValue);
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertNotNull(callback.mTrailers);
        // Verify that header value is properly echoed in trailers.
        assertThat(callback.mTrailers.getAsMap())
                .containsEntry("echo-" + headerName, Arrays.asList(headerValue));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCustomUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoHeaderUrl(userAgentName), callback, callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.addHeader(userAgentName, userAgentValue);
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCustomCronetEngineUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";
        ExperimentalCronetEngine.Builder engineBuilder =
                new ExperimentalCronetEngine.Builder(getContext());
        engineBuilder.setUserAgent(userAgentValue);
        CronetTestUtil.setMockCertVerifierForTesting(
                engineBuilder, QuicTestServer.createMockCertVerifier());
        ExperimentalCronetEngine engine = engineBuilder.build();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = engine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoHeaderUrl(userAgentName), callback, callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testDefaultUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoHeaderUrl(userAgentName), callback, callback.getExecutor());
        builder.setHttpMethod("GET");
        builder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo(new CronetEngine.Builder(getContext()).getDefaultUserAgent());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .addHeader("foo", "Value with Spaces")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-foo", Arrays.asList("Value with Spaces"));
        assertThat(callback.mResponseInfo.getAllHeaders())
                .containsEntry("echo-content-type", Arrays.asList("zebra"));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testEchoStreamEmptyWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData(new byte[0]);
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEmpty();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testDoubleWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testDoubleRead() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
            @Override
            public void onResponseHeadersReceived(
                    BidirectionalStream stream, UrlResponseInfo info) {
                startNextRead(stream);
                try {
                    // Second read from callback invoked on single-threaded executor throws
                    // an exception because previous read is still pending until its completion
                    // is handled on executor.
                    stream.read(ByteBuffer.allocateDirect(5));
                    fail("Exception is not thrown.");
                } catch (Exception e) {
                    assertThat(e.getMessage()).isEqualTo("Unexpected read attempt.");
                }
            }
        };
        callback.addWriteData("1".getBytes());
        callback.addWriteData("2".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testReadAndWrite() throws Exception {
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
            @Override
            public void onResponseHeadersReceived(
                    BidirectionalStream stream, UrlResponseInfo info) {
                // Start the write, that will not complete until callback completion.
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.waitForNextWriteStep();
        callback.waitForNextReadStep();
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("12");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        // Write first.
        callback.waitForNextWriteStep(); // onStreamReady
        for (String expected : testData) {
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
        assertTrue(callback.mResponseAsString.startsWith(testData[0]));
        assertTrue(stringData.toString().startsWith(callback.mResponseAsString));
        // Read the rest of the response.
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
                mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
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
            assertFalse(stream.isDone());
        }

        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo(stringData.toString());
    }

    /**
     * Checks that the buffer is updated correctly, when starting at an offset.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSimpleGetBufferUpdates() throws Exception {
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        // Since the method is "GET", the expected response body is also "GET".
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();

        assertThat(callback.mError).isNull();
        assertFalse(callback.isDone());
        assertThat(callback.mResponseStep)
                .isEqualTo(TestBidirectionalStreamCallback.ResponseStep.ON_RESPONSE_STARTED);

        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        readBuffer.put("FOR".getBytes());
        assertThat(readBuffer.position()).isEqualTo(3);

        // Read first two characters of the response ("GE"). It's theoretically
        // possible to need one read per character, though in practice,
        // shouldn't happen.
        while (callback.mResponseAsString.length() < 2) {
            assertFalse(callback.isDone());
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

        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
        checkResponseInfo(callback.mResponseInfo, Http2TestServer.getEchoMethodUrl(), 200, "");

        // Check that buffer contents were not modified.
        assertThat(bufferContentsToString(readBuffer, 0, 5)).isEqualTo("FORTE");

        // Position should not have been modified, since nothing was read.
        assertThat(readBuffer.position()).isEqualTo(1);
        // Limit should be unchanged as always.
        assertThat(readBuffer.limit()).isEqualTo(5);

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_SUCCEEDED);

        // TestRequestFinishedListener expects a single call to onRequestFinished. Here we
        // explicitly wait for the call to happen to avoid a race condition with the other
        // TestRequestFinishedListener created within runSimpleGetWithExpectedReceivedByteCount.
        requestFinishedListener.blockUntilDone();
        mCronetEngine.removeRequestFinishedListener(requestFinishedListener);

        // Make sure there are no other pending messages, which would trigger
        // asserts in TestBidirectionalCallback.
        // The expected received bytes count is lower than it would be for the first request on the
        // connection, because the server includes an HPACK dynamic table size update only in the
        // first response HEADERS frame.
        runSimpleGetWithExpectedReceivedByteCount(27);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBadBuffers() throws Exception {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();

        assertThat(callback.mError).isNull();
        assertFalse(callback.isDone());
        assertThat(callback.mResponseStep)
                .isEqualTo(TestBidirectionalStreamCallback.ResponseStep.ON_RESPONSE_STARTED);

        // Try to read using a full buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocateDirect(4);
            readBuffer.put("full".getBytes());
            stream.read(readBuffer);
            fail("Exception not thrown");
        } catch (IllegalArgumentException e) {
            assertThat(e.getMessage()).isEqualTo("ByteBuffer is already full.");
        }

        // Try to read using a non-direct buffer.
        try {
            ByteBuffer readBuffer = ByteBuffer.allocate(5);
            stream.read(readBuffer);
            fail("Exception not thrown");
        } catch (Exception e) {
            assertThat(e).hasMessageThat().isEqualTo("byteBuffer must be a direct ByteBuffer.");
        }

        // Finish the stream with a direct ByteBuffer.
        callback.setAutoAdvance(true);
        ByteBuffer readBuffer = ByteBuffer.allocateDirect(5);
        stream.read(readBuffer);
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("GET");
    }

    private void throwOrCancel(
            FailureType failureType, ResponseStep failureStep, boolean expectError) {
        // Use a fresh CronetEngine each time so Http2 session is not reused.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());
        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());
        mCronetEngine = builder.build();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setFailure(failureType, failureStep);
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        BidirectionalStream.Builder streamBuilder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = streamBuilder.setHttpMethod("GET").build();
        Date startTime = new Date();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        RequestFinishedInfo.Metrics metrics = finishedInfo.getMetrics();
        assertNotNull(metrics);
        // Cancellation when stream is ready does not guarantee that
        // mResponseInfo is null because there might be a
        // onResponseHeadersReceived already queued in the executor.
        // See crbug.com/594432.
        if (failureStep != ResponseStep.ON_STREAM_READY) {
            assertNotNull(callback.mResponseInfo);
        }
        // Check metrics information.
        if (failureStep == ResponseStep.ON_RESPONSE_STARTED
                || failureStep == ResponseStep.ON_READ_COMPLETED
                || failureStep == ResponseStep.ON_TRAILERS) {
            // For steps after response headers are received, there will be
            // connect timing metrics.
            MetricsTestUtil.checkTimingMetrics(metrics, startTime, endTime);
            MetricsTestUtil.checkHasConnectTiming(metrics, startTime, endTime, true);
            assertThat(metrics.getSentByteCount()).isGreaterThan(0L);
            assertThat(metrics.getReceivedByteCount()).isGreaterThan(0L);
        } else if (failureStep == ResponseStep.ON_STREAM_READY) {
            assertNotNull(metrics.getRequestStart());
            MetricsTestUtil.assertAfter(metrics.getRequestStart(), startTime);
            assertNotNull(metrics.getRequestEnd());
            MetricsTestUtil.assertAfter(endTime, metrics.getRequestEnd());
            MetricsTestUtil.assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
        }
        assertThat(callback.mError != null).isEqualTo(expectError);
        assertThat(callback.mOnErrorCalled).isEqualTo(expectError);
        if (expectError) {
            assertNotNull(finishedInfo.getException());
            assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.FAILED);
        } else {
            assertNull(finishedInfo.getException());
            assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.CANCELED);
        }
        assertThat(callback.mOnCanceledCalled)
                .isEqualTo(failureType == FailureType.CANCEL_SYNC
                        || failureType == FailureType.CANCEL_ASYNC
                        || failureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE);
        mCronetEngine.removeRequestFinishedListener(requestFinishedListener);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet
    public void testThrowOnSucceeded() {
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_SUCCEEDED);
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        BidirectionalStream stream = builder.setHttpMethod("GET").build();
        stream.start();
        callback.blockForDone();
        assertThat(ResponseStep.ON_SUCCEEDED).isEqualTo(callback.mResponseStep);
        assertTrue(stream.isDone());
        assertNotNull(callback.mResponseInfo);
        // Check that error thrown from 'onSucceeded' callback is not reported.
        assertNull(callback.mError);
        assertFalse(callback.mOnErrorCalled);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testExecutorShutdownBeforeStreamIsDone() {
        // Test that stream is destroyed even if executor is shut down and rejects posting tasks.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) builder.setHttpMethod("GET").build();
        stream.start();
        callback.waitForNextReadStep();
        assertFalse(callback.isDone());
        assertFalse(stream.isDone());

        final ConditionVariable streamDestroyed = new ConditionVariable(false);
        stream.setOnDestroyedCallbackForTesting(new Runnable() {
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

        assertFalse(callback.isDone());
        assertTrue(stream.isDone());
    }

    /**
     * Callback that shuts down the engine when the stream has succeeded
     * or failed.
     */
    private class ShutdownTestBidirectionalStreamCallback extends TestBidirectionalStreamCallback {
        @Override
        public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
            mCronetEngine.shutdown();
            // Clear mCronetEngine so it doesn't get shut down second time in tearDown().
            mCronetEngine = null;
            super.onSucceeded(stream, info);
        }

        @Override
        public void onFailed(
                BidirectionalStream stream, UrlResponseInfo info, CronetException error) {
            mCronetEngine.shutdown();
            // Clear mCronetEngine so it doesn't get shut down second time in tearDown().
            mCronetEngine = null;
            super.onFailed(stream, info, error);
        }

        @Override
        public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
            mCronetEngine.shutdown();
            // Clear mCronetEngine so it doesn't get shut down second time in tearDown().
            mCronetEngine = null;
            super.onCanceled(stream, info);
        }
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCronetEngineShutdown() throws Exception {
        // Test that CronetEngine cannot be shut down if there are any active streams.
        TestBidirectionalStreamCallback callback = new ShutdownTestBidirectionalStreamCallback();
        // Block callback when response starts to verify that shutdown fails
        // if there are active streams.
        callback.setAutoAdvance(false);
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) builder.setHttpMethod("GET").build();
        stream.start();
        try {
            mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");
        }

        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        try {
            mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");
        }
        callback.startNextRead(stream);

        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);
        try {
            mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");
        }

        // May not have read all the data, in theory. Just enable auto-advance
        // and finish the request.
        callback.setAutoAdvance(true);
        callback.startNextRead(stream);
        callback.blockForDone();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCronetEngineShutdownAfterStreamFailure() throws Exception {
        // Test that CronetEngine can be shut down after stream reports a failure.
        TestBidirectionalStreamCallback callback = new ShutdownTestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) builder.setHttpMethod("GET").build();
        stream.start();
        callback.setFailure(FailureType.THROW_SYNC, ResponseStep.ON_READ_COMPLETED);
        callback.blockForDone();
        assertTrue(callback.mOnErrorCalled);
        assertNull(mCronetEngine);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testCronetEngineShutdownAfterStreamCancel() throws Exception {
        // Test that CronetEngine can be shut down after stream is canceled.
        TestBidirectionalStreamCallback callback = new ShutdownTestBidirectionalStreamCallback();
        BidirectionalStream.Builder builder = mCronetEngine.newBidirectionalStreamBuilder(
                Http2TestServer.getEchoMethodUrl(), callback, callback.getExecutor());
        CronetBidirectionalStream stream =
                (CronetBidirectionalStream) builder.setHttpMethod("GET").build();

        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        stream.start();
        try {
            mCronetEngine.shutdown();
            fail("Should throw an exception");
        } catch (Exception e) {
            assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");
        }
        callback.waitForNextReadStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        stream.cancel();
        callback.blockForDone();
        assertTrue(callback.mOnCanceledCalled);
        assertNull(mCronetEngine);
    }

    /*
     * Verifies NetworkException constructed from specific error codes are retryable.
     */
    @SmallTest
    @Test
    @OnlyRunNativeCronet
    public void testErrorCodes() throws Exception {
        // Non-BidirectionalStream specific error codes.
        checkSpecificErrorCode(NetError.ERR_NAME_NOT_RESOLVED,
                NetworkException.ERROR_HOSTNAME_NOT_RESOLVED, false);
        checkSpecificErrorCode(NetError.ERR_INTERNET_DISCONNECTED,
                NetworkException.ERROR_INTERNET_DISCONNECTED, false);
        checkSpecificErrorCode(
                NetError.ERR_NETWORK_CHANGED, NetworkException.ERROR_NETWORK_CHANGED, true);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_CLOSED, NetworkException.ERROR_CONNECTION_CLOSED, true);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_REFUSED, NetworkException.ERROR_CONNECTION_REFUSED, false);
        checkSpecificErrorCode(
                NetError.ERR_CONNECTION_RESET, NetworkException.ERROR_CONNECTION_RESET, true);
        checkSpecificErrorCode(NetError.ERR_CONNECTION_TIMED_OUT,
                NetworkException.ERROR_CONNECTION_TIMED_OUT, true);
        checkSpecificErrorCode(NetError.ERR_TIMED_OUT, NetworkException.ERROR_TIMED_OUT, true);
        checkSpecificErrorCode(NetError.ERR_ADDRESS_UNREACHABLE,
                NetworkException.ERROR_ADDRESS_UNREACHABLE, false);
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
    @OnlyRunNativeCronet
    @RequiresMinApi(10) // Tagging support added in API level 10: crrev.com/c/chromium/src/+/937583
    @RequiresMinAndroidApi(Build.VERSION_CODES.M) // crbug/1301957
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
        mCronetEngine.newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
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

    /**
     * Cronet does not currently provide an API to wait for the active request
     * count to change. We can't just wait for the terminal callback to fire
     * because Cronet updates the count some time *after* we return from the
     * callback. We hack around this by polling the active request count in a
     * loop.
     */
    private void waitForActiveRequestCount(int expectedCount) throws Exception {
        while (mCronetEngine.getActiveRequestCount() != expectedCount) Thread.sleep(100);
    }
}
