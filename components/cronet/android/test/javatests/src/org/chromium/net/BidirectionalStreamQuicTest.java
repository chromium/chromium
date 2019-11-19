// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.CollectionUtil.newHashSet;
import static org.chromium.net.CronetTestRule.getContext;

import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.MetricsTestUtil.TestRequestFinishedListener;

import java.nio.ByteBuffer;
import java.util.Date;
import java.util.HashSet;

/**
 * Tests functionality of BidirectionalStream's QUIC implementation.
 */
@RunWith(AndroidJUnit4.class)
public class BidirectionalStreamQuicTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private ExperimentalCronetEngine mCronetEngine;
    private enum QuicBidirectionalStreams {
        ENABLED,
        DISABLED,
    }

    private void setUp(QuicBidirectionalStreams enabled) throws Exception {
        // Load library first to create MockCertVerifier.
        System.loadLibrary("cronet_tests");
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(getContext());

        QuicTestServer.startQuicTestServer(getContext());

        builder.enableQuic(true);
        JSONObject quicParams = new JSONObject();
        if (enabled == QuicBidirectionalStreams.DISABLED) {
            quicParams.put("quic_disable_bidirectional_streams", true);
        }
        JSONObject hostResolverParams = CronetTestUtil.generateHostResolverRules();
        JSONObject experimentalOptions = new JSONObject()
                                                 .put("QUIC", quicParams)
                                                 .put("HostResolverRules", hostResolverParams);
        builder.setExperimentalOptions(experimentalOptions.toString());

        builder.addQuicHint(QuicTestServer.getServerHost(), QuicTestServer.getServerPort(),
                QuicTestServer.getServerPort());

        CronetTestUtil.setMockCertVerifierForTesting(
                builder, QuicTestServer.createMockCertVerifier());

        mCronetEngine = builder.build();
    }

    @After
    public void tearDown() throws Exception {
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Test that QUIC is negotiated.
    public void testSimpleGet() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("This is a simple text file served by QUIC.\n", callback.mResponseAsString);
        assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSimplePost() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Although we have no way to verify data sent at this point, this test
        // can make sure that onWriteCompleted is invoked appropriately.
        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        mCronetEngine.addRequestFinishedListener(requestFinishedListener);
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .addRequestAnnotation("request annotation")
                        .addRequestAnnotation(this)
                        .build();
        Date startTime = new Date();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(finishedInfo, quicURL, startTime, endTime);
        assertEquals(RequestFinishedInfo.SUCCEEDED, finishedInfo.getFinishedReason());
        MetricsTestUtil.checkHasConnectTiming(finishedInfo.getMetrics(), startTime, endTime, true);
        assertEquals(newHashSet("request annotation", this),
                new HashSet<Object>(finishedInfo.getAnnotations()));
        assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
        assertEquals("This is a simple text file served by QUIC.\n", callback.mResponseAsString);
        assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSimplePostWithFlush() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String path = "/simple.txt";
            String quicURL = QuicTestServer.getServerURL() + path;
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            // Although we have no way to verify data sent at this point, this test
            // can make sure that onWriteCompleted is invoked appropriately.
            callback.addWriteData("Test String".getBytes(), false);
            callback.addWriteData("1234567890".getBytes(), false);
            callback.addWriteData("woot!".getBytes(), true);
            BidirectionalStream stream = mCronetEngine
                                                 .newBidirectionalStreamBuilder(
                                                         quicURL, callback, callback.getExecutor())
                                                 .delayRequestHeadersUntilFirstFlush(i == 0)
                                                 .addHeader("foo", "bar")
                                                 .addHeader("empty", "")
                                                 .addHeader("Content-Type", "zebra")
                                                 .build();
            stream.start();
            callback.blockForDone();
            assertTrue(stream.isDone());
            assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
            assertEquals(
                    "This is a simple text file served by QUIC.\n", callback.mResponseAsString);
            assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSimplePostWithFlushTwice() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String path = "/simple.txt";
            String quicURL = QuicTestServer.getServerURL() + path;
            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
            // Although we have no way to verify data sent at this point, this test
            // can make sure that onWriteCompleted is invoked appropriately.
            callback.addWriteData("Test String".getBytes(), false);
            callback.addWriteData("1234567890".getBytes(), false);
            callback.addWriteData("woot!".getBytes(), true);
            callback.addWriteData("Test String".getBytes(), false);
            callback.addWriteData("1234567890".getBytes(), false);
            callback.addWriteData("woot!".getBytes(), true);
            BidirectionalStream stream = mCronetEngine
                                                 .newBidirectionalStreamBuilder(
                                                         quicURL, callback, callback.getExecutor())
                                                 .delayRequestHeadersUntilFirstFlush(i == 0)
                                                 .addHeader("foo", "bar")
                                                 .addHeader("empty", "")
                                                 .addHeader("Content-Type", "zebra")
                                                 .build();
            stream.start();
            callback.blockForDone();
            assertTrue(stream.isDone());
            assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
            assertEquals(
                    "This is a simple text file served by QUIC.\n", callback.mResponseAsString);
            assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSimpleGetWithFlush() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String path = "/simple.txt";
            String url = QuicTestServer.getServerURL() + path;

            TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
                @Override
                public void onStreamReady(BidirectionalStream stream) {
                    // This flush should send the delayed headers.
                    stream.flush();
                    super.onStreamReady(stream);
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

            assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
            assertEquals(
                    "This is a simple text file served by QUIC.\n", callback.mResponseAsString);
            assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testSimplePostWithFlushAfterOneWrite() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String path = "/simple.txt";
            String url = QuicTestServer.getServerURL() + path;

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

            assertEquals(200, callback.mResponseInfo.getHttpStatusCode());
            assertEquals(
                    "This is a simple text file served by QUIC.\n", callback.mResponseAsString);
            assertEquals("quic/1+spdy/3", callback.mResponseInfo.getNegotiatedProtocol());
        }
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testQuicBidirectionalStreamDisabled() throws Exception {
        setUp(QuicBidirectionalStreams.DISABLED);
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;

        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertTrue(callback.mOnErrorCalled);
        assertNull(callback.mResponseInfo);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    // Tests that if the stream failed between the time when we issue a Write()
    // and when the Write() is executed in the native stack, there is no crash.
    // This test is racy, but it should catch a crash (if there is any) most of
    // the time.
    public void testStreamFailBeforeWriteIsExecutedOnNetworkThread() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;

        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
            @Override
            public void onWriteCompleted(BidirectionalStream stream, UrlResponseInfo info,
                    ByteBuffer buffer, boolean endOfStream) {
                // Super class will write the next piece of data.
                super.onWriteCompleted(stream, info, buffer, endOfStream);
                // Shut down the server, and the stream should error out.
                // The second call to shutdownQuicTestServer is no-op.
                QuicTestServer.shutdownQuicTestServer();
            }
        };

        callback.addWriteData("Test String".getBytes());
        callback.addWriteData("1234567890".getBytes());
        callback.addWriteData("woot!".getBytes());

        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .addHeader("foo", "bar")
                        .addHeader("empty", "")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        // Server terminated on us, so the stream must fail.
        // QUIC reports this as ERR_QUIC_PROTOCOL_ERROR. Sometimes we get ERR_CONNECTION_REFUSED.
        assertNotNull(callback.mError);
        assertTrue(callback.mError instanceof NetworkException);
        NetworkException networkError = (NetworkException) callback.mError;
        assertTrue(NetError.ERR_QUIC_PROTOCOL_ERROR == networkError.getCronetInternalErrorCode()
                || NetError.ERR_CONNECTION_REFUSED == networkError.getCronetInternalErrorCode());
        if (NetError.ERR_CONNECTION_REFUSED == networkError.getCronetInternalErrorCode()) return;
        assertTrue(callback.mError instanceof QuicException);
    }

    @Test
    @SmallTest
    @Feature({"Cronet"})
    @OnlyRunNativeCronet
    public void testStreamFailWithQuicDetailedErrorCode() throws Exception {
        setUp(QuicBidirectionalStreams.ENABLED);
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback() {
            @Override
            public void onStreamReady(BidirectionalStream stream) {
                // Shut down the server, and the stream should error out.
                // The second call to shutdownQuicTestServer is no-op.
                QuicTestServer.shutdownQuicTestServer();
            }
        };
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .delayRequestHeadersUntilFirstFlush(true)
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertTrue(stream.isDone());
        assertNotNull(callback.mError);
        assertTrue(callback.mError instanceof QuicException);
        QuicException quicException = (QuicException) callback.mError;
        // Checks that detailed quic error code is not QUIC_NO_ERROR == 0.
        assertTrue("actual error " + quicException.getQuicDetailedErrorCode(),
                0 < quicException.getQuicDetailedErrorCode());
    }
}
