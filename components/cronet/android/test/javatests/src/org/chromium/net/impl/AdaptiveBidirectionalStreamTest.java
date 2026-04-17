// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetLoggerTestRule;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.StringFlag;
import org.chromium.net.CronetTestUtil;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.Http2TestServer;
import org.chromium.net.NetworkException;
import org.chromium.net.QuicTestServer;
import org.chromium.net.TestBidirectionalStreamCallback;

import java.net.SocketAddress;

/** Test functionality of BidirectionalStream interface. */
@DoNotBatch(reason = "crbug.com/1459563")
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "The fallback implementation doesn't support adaptive bidirectional streaming")
public class AdaptiveBidirectionalStreamTest {
    private static final String TAG = AdaptiveBidirectionalStreamTest.class.getSimpleName();

    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private ExperimentalCronetEngine mCronetEngine;
    private final CronetLoggerTestRule<TestLogger> mLoggerTestRule =
            new CronetLoggerTestRule<>(TestLogger.class);

    @Rule public final RuleChain chain = RuleChain.outerRule(mLoggerTestRule).around(mTestRule);

    private TestLogger mTestLogger;

    private SocketDroppingPacketHandler mDroppingPacketHandler;

    @ChannelHandler.Sharable
    private static final class SocketDroppingPacketHandler extends ChannelInboundHandlerAdapter {
        private boolean mDropFirstRemoteAddress;
        private SocketAddress mDroppedRemoteAddress;

        @Override
        public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
            if (mDropFirstRemoteAddress && mDroppedRemoteAddress == null) {
                mDroppedRemoteAddress = ctx.channel().remoteAddress();
            }
            if (mDroppedRemoteAddress != null
                    && mDroppedRemoteAddress == ctx.channel().remoteAddress()) {
                Log.i(TAG, "Dropping packet for socket " + msg);
                return;
            }
            ctx.fireChannelRead(msg);
        }
    }
    ;

    @Before
    public void setUp() throws Exception {
        mTestLogger = mLoggerTestRule.mTestLogger;
        mDroppingPacketHandler = new SocketDroppingPacketHandler();
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
        assertThat(
                        Http2TestServer.startHttp2TestServer(
                                new Http2TestServer.ServerStartOptions(
                                                mTestRule.getTestFramework().getContext())
                                        .setPreTlsPacketHandler(mDroppingPacketHandler)))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream")
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void postViaBidirectionalStreamAdaptiveHost_success() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();

        assertThat(stream.isDone()).isTrue();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String");
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream")
            })
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void tlsConnectionFailsNoFallback_throwsConnectionTimeoutError() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // Drop packet for first socket we find.
        mDroppingPacketHandler.mDropFirstRemoteAddress = true;
        String url = Http2TestServer.getEchoStreamUrl();
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();

        // We caught an error.
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mError).isInstanceOf(NetworkException.class);
        NetworkException networkException = (NetworkException) callback.mError;
        assertThat(networkException.getErrorCode()).isEqualTo(NetworkException.ERROR_TIMED_OUT);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost,https://localhost2"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream,/echostream2")
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void adaptiveNetworkPaths() throws Exception {
        CronetAdaptiveRequestContext adaptiveRequestContext =
                new CronetAdaptiveRequestContext(mTestRule.getTestFramework().getContext());

        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost/echostream"))
                .isTrue();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost2/echostream2"))
                .isTrue();
        assertThat(
                        adaptiveRequestContext.isAdaptiveNetworkUrl(
                                "https://localhost2:8080/echostream2"))
                .isTrue();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost/otherstream"))
                .isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost2/otherstream"))
                .isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://otherhost/echostream"))
                .isFalse();
        assertThat(
                        adaptiveRequestContext.isAdaptiveNetworkUrl(
                                "a QA engineer walks into a bar and orders -1 beer"))
                .isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("")).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = ""),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "")
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void adaptiveNetworkPaths_empty() throws Exception {
        CronetAdaptiveRequestContext adaptiveRequestContext =
                new CronetAdaptiveRequestContext(mTestRule.getTestFramework().getContext());

        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost/echostream"))
                .isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("")).isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://localhost/otherstream"))
                .isFalse();
        assertThat(adaptiveRequestContext.isAdaptiveNetworkUrl("https://otherhost/echostream"))
                .isFalse();
    }
}
