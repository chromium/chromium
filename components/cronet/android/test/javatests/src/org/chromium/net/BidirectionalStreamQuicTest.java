// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.TestLogger;

import java.nio.ByteBuffer;
import java.util.Date;

/** Tests functionality of BidirectionalStream's QUIC implementation. */
@RunWith(AndroidJUnit4.class)
// TODO(b/344966604): Fix and batch afterwards.
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
        reason =
                "The fallback implementation doesn't support bidirectional streaming. "
                        + "crbug.com/1494870: Enable for AOSP_PLATFORM once fixed")
public class BidirectionalStreamQuicTest {
    private final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();
    private final CronetLoggerTestRule<TestLogger> mLoggerTestRule =
            new CronetLoggerTestRule<>(TestLogger.class);

    @Rule public final RuleChain chain = RuleChain.outerRule(mTestRule).around(mLoggerTestRule);

    private TestLogger mTestLogger;

    private ExperimentalCronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        mTestLogger = mLoggerTestRule.mTestLogger;
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            QuicTestServer.startQuicTestServer(
                                    mTestRule.getTestFramework().getContext());

                            JSONObject quicParams =
                                    new JSONObject()
                                            .put("retry_without_alt_svc_on_quic_errors", false);
                            JSONObject hostResolverParams =
                                    CronetTestUtil.generateHostResolverRules();
                            JSONObject experimentalOptions =
                                    new JSONObject()
                                            .put("QUIC", quicParams)
                                            .put("HostResolverRules", hostResolverParams);
                            builder.setExperimentalOptions(experimentalOptions.toString())
                                    .addQuicHint(
                                            QuicTestServer.getServerHost(),
                                            QuicTestServer.getServerPort(),
                                            QuicTestServer.getServerPort());

                            CronetTestUtil.setMockCertVerifierForTesting(
                                    builder, QuicTestServer.createMockCertVerifier());
                        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
    }

    @After
    public void tearDown() throws Exception {
        mTestLogger = null;
        QuicTestServer.shutdownQuicTestServer();
    }

    @Test
    @SmallTest
    // Test that QUIC is negotiated.
    public void testSimpleGet() throws Exception {
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
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo("This is a simple text file served by QUIC.\n");
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("quic/1+spdy/3");
    }

    @Test
    @SmallTest
    public void testSimplePost() throws Exception {
        CronetImplementation implementationUnderTest = mTestRule.implementationUnderTest();
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
        assertThat(stream.isDone()).isTrue();
        requestFinishedListener.blockUntilDone();
        Date endTime = new Date();
        RequestFinishedInfo finishedInfo = requestFinishedListener.getRequestInfo();
        MetricsTestUtil.checkRequestFinishedInfo(
                implementationUnderTest, finishedInfo, quicURL, startTime, endTime);
        assertThat(finishedInfo.getFinishedReason()).isEqualTo(RequestFinishedInfo.SUCCEEDED);
        MetricsTestUtil.checkHasConnectTiming(
                implementationUnderTest, finishedInfo.getMetrics(), startTime, endTime, true);
        assertThat(finishedInfo.getAnnotations()).containsExactly("request annotation", this);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString)
                .isEqualTo("This is a simple text file served by QUIC.\n");
        assertThat(callback.getResponseInfoWithChecks())
                .hasNegotiatedProtocolThat()
                .isEqualTo("quic/1+spdy/3");
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlush() throws Exception {
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
            BidirectionalStream stream =
                    mCronetEngine
                            .newBidirectionalStreamBuilder(
                                    quicURL, callback, callback.getExecutor())
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
                    .isEqualTo("This is a simple text file served by QUIC.\n");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasNegotiatedProtocolThat()
                    .isEqualTo("quic/1+spdy/3");
        }
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlushTwice() throws Exception {
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
            BidirectionalStream stream =
                    mCronetEngine
                            .newBidirectionalStreamBuilder(
                                    quicURL, callback, callback.getExecutor())
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
                    .isEqualTo("This is a simple text file served by QUIC.\n");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasNegotiatedProtocolThat()
                    .isEqualTo("quic/1+spdy/3");
        }
    }

    @Test
    @SmallTest
    public void testSimpleGetWithFlush() throws Exception {
        // TODO(xunjieli): Use ParameterizedTest instead of the loop.
        for (int i = 0; i < 2; i++) {
            String path = "/simple.txt";
            String url = QuicTestServer.getServerURL() + path;

            TestBidirectionalStreamCallback callback =
                    new TestBidirectionalStreamCallback() {
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
            assertThat(stream.isDone()).isTrue();

            // Flush after stream is completed is no-op. It shouldn't call into the destroyed
            // adapter.
            stream.flush();

            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            assertThat(callback.mResponseAsString)
                    .isEqualTo("This is a simple text file served by QUIC.\n");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasNegotiatedProtocolThat()
                    .isEqualTo("quic/1+spdy/3");
        }
    }

    @Test
    @SmallTest
    public void testSimplePostWithFlushAfterOneWrite() throws Exception {
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
            assertThat(stream.isDone()).isTrue();

            assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
            assertThat(callback.mResponseAsString)
                    .isEqualTo("This is a simple text file served by QUIC.\n");
            assertThat(callback.getResponseInfoWithChecks())
                    .hasNegotiatedProtocolThat()
                    .isEqualTo("quic/1+spdy/3");
        }
    }

    @Test
    @SmallTest
    // Tests that if the stream failed between the time when we issue a Write()
    // and when the Write() is executed in the native stack, there is no crash.
    // This test is racy, but it should catch a crash (if there is any) most of
    // the time.
    public void testStreamFailBeforeWriteIsExecutedOnNetworkThread() throws Exception {
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;

        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
                    @Override
                    public void onWriteCompleted(
                            BidirectionalStream stream,
                            UrlResponseInfo info,
                            ByteBuffer buffer,
                            boolean endOfStream) {
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
        assertThat(stream.isDone()).isTrue();
        // Server terminated on us, so the stream must fail.
        // QUIC reports this as ERR_QUIC_PROTOCOL_ERROR. Sometimes we get ERR_CONNECTION_REFUSED.
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkError = (NetworkException) callback.mError;
        assertThat(networkError.getCronetInternalErrorCode())
                .isAnyOf(NetError.ERR_QUIC_PROTOCOL_ERROR, NetError.ERR_CONNECTION_REFUSED);
        if (NetError.ERR_CONNECTION_REFUSED == networkError.getCronetInternalErrorCode()) return;
        assertThat(callback.mError).isInstanceOf(QuicException.class);
    }

    @Test
    @SmallTest
    public void testServerAbruptShutdownShouldTerminateConnection() throws Exception {
        String path = "/simple.txt";
        String quicURL = QuicTestServer.getServerURL() + path;
        TestBidirectionalStreamCallback callback =
                new TestBidirectionalStreamCallback() {
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
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.mError).isNotNull();
        // TODO(b/365787337): Investigate the flaky QUIC error code when the server shuts down
        // abruptly.
        // The network error returned when a server shutdowns abruptly is non-deterministic
        // for the test server that we are using (QuicTestServer). It could return one of the
        // following:
        // (1) QuicException with error code (51) PACKET_READ_ERROR.
        // (2) QuicException with error code (16) PEER_GOING_AWAY.
        // (3) BidirectionalStreamNetworkException with internal error code
        // ERR_QUIC_GOAWAY_REQUEST_CAN_BE_RETRIED.
        // That's why there is no explicit check for the error code.
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
    }

    @Test
    @SmallTest
    public void testServerSendQuicConnectionCloseCorrectlyReported() throws Exception {
        String quicURL = QuicTestServer.getServerURL() + QuicTestServer.getConnectionClosePath();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(quicURL, callback, callback.getExecutor())
                        .setHttpMethod("GET")
                        .addHeader("Content-Type", "zebra")
                        .build();
        stream.start();
        callback.blockForDone();
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.mError).isNotNull();

        assertThat(callback.mError).isInstanceOf(QuicException.class);
        QuicException quicException = (QuicException) callback.mError;
        // 0 is QUIC_NO_ERROR, This is expected because of the test-server behavior, see
        // https://source.chromium.org/chromium/_/quiche/quiche/+/86e3e869377b05a7143dfa07a4d1219881396661:quiche/quic/tools/quic_simple_server_stream.cc;l=286;
        assertThat(quicException.getQuicDetailedErrorCode()).isEqualTo(0);
        assertThat(quicException.getConnectionCloseSource()).isEqualTo(ConnectionCloseSource.PEER);
        assertThat(quicException.getErrorCode())
                .isEqualTo(NetworkException.ERROR_QUIC_PROTOCOL_FAILED);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mTestLogger.waitForLogCronetTrafficInfo();
            final CronetTrafficInfo trafficInfo = mTestLogger.getLastCronetTrafficInfo();
            assertThat(trafficInfo.getConnectionCloseSource())
                    .isEqualTo(quicException.getConnectionCloseSource());
            assertThat(trafficInfo.getNetworkInternalErrorCode())
                    .isEqualTo(quicException.getCronetInternalErrorCode());
            assertThat(trafficInfo.getQuicErrorCode())
                    .isEqualTo(quicException.getQuicDetailedErrorCode());
            assertThat(trafficInfo.getFailureReason())
                    .isEqualTo(CronetTrafficInfo.RequestFailureReason.NETWORK);
        }
    }
}
