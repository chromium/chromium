// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.net.Network;
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
import org.chromium.net.ConnectivityManagerWrapper;
import org.chromium.net.CronetLoggerTestRule;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.BoolFlag;
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
import java.net.URI;

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
    private SocketDroppingPacketHandler mPostTlsDroppingPacketHandler;
    private CronetAdaptiveRequestContext mAdaptiveRequestContext;
    private ConnectivityManagerWrapper mMockConnectivityManagerWrapper;
    private Network mDefaultNetwork;
    private long mDefaultNetworkHandle;

    @ChannelHandler.Sharable
    private static final class SocketDroppingPacketHandler extends ChannelInboundHandlerAdapter {
        private boolean mDropFirstRemoteAddress;
        private boolean mDropAllRemoteAddresses;
        private SocketAddress mDroppedRemoteAddress;

        @Override
        public void channelRead(ChannelHandlerContext ctx, Object msg) throws Exception {
            if (mDropFirstRemoteAddress && mDroppedRemoteAddress == null) {
                mDroppedRemoteAddress = ctx.channel().remoteAddress();
            }
            if (mDropAllRemoteAddresses
                    || (mDroppedRemoteAddress != null
                            && mDroppedRemoteAddress == ctx.channel().remoteAddress())) {
                Log.i(TAG, "Dropping packet for socket " + ctx.channel().remoteAddress());
                return;
            }
            ctx.fireChannelRead(msg);
        }
    }
    ;

    @Before
    public void setUp() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mTestLogger = mLoggerTestRule.mTestLogger;
        mDroppingPacketHandler = new SocketDroppingPacketHandler();
        mPostTlsDroppingPacketHandler = new SocketDroppingPacketHandler();

        ExperimentalCronetEngine.Builder builder =
                (ExperimentalCronetEngine.Builder)
                        new NativeCronetProvider(mTestRule.getTestFramework().getContext())
                                .createBuilder();
        // TODO(crbug.com/40284777): Fallback to MockCertVerifier when custom CAs are not supported.
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        }
        mCronetEngine = (ExperimentalCronetEngine) builder.build();
        assertThat(
                        Http2TestServer.startHttp2TestServer(
                                new Http2TestServer.ServerStartOptions(
                                                mTestRule.getTestFramework().getContext())
                                        .setPreTlsPacketHandler(mDroppingPacketHandler)
                                        .setPostTlsPacketHandler(mPostTlsDroppingPacketHandler)))
                .isTrue();

        mAdaptiveRequestContext = ((CronetUrlRequestContext) mCronetEngine).mAdaptiveRequestContext;
        mMockConnectivityManagerWrapper = mock(ConnectivityManagerWrapper.class);
        mAdaptiveRequestContext.setConnectivityManagerWrapperForTest(
                mMockConnectivityManagerWrapper);

        ConnectivityManagerWrapper realWrapper =
                new ConnectivityManagerWrapper(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = realWrapper.getDefaultNetwork();
        if (defaultNetwork == null) {
            mDefaultNetwork = mock(Network.class);
            mDefaultNetworkHandle = 2L;
            when(mDefaultNetwork.getNetworkHandle()).thenReturn(mDefaultNetworkHandle);
        } else {
            mDefaultNetwork = defaultNetwork;
            mDefaultNetworkHandle = defaultNetwork.getNetworkHandle();
        }

        // Setup the mock to return an alternative network by default.
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(null);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mDefaultNetwork});
    }

    @After
    public void tearDown() throws Exception {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
        }
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
                        value = "/echostream"),
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Logging is not supported for these implementations.")
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void postViaBidirectionalStreamWithFallbackSet_successOnPrimaryNetwork()
            throws Exception {
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

        mTestLogger.waitForLogCronetAdaptiveTrafficTerminated();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo()).isNotNull();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo().getWinner())
                .isEqualTo(
                        CronetLogger.CronetAdaptiveTrafficWinner
                                .CRONET_ADAPTIVE_TRAFFIC_WINNER_MAIN);
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void tlsConnectionFailsAllNetworks_throwsConnectionTimeoutError() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // All packets being dropped for all networks. We can't save this.
        mDroppingPacketHandler.mDropAllRemoteAddresses = true;
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

    // TODO(b/474048542): Move this to CronetAdaptiveRequestContextTest.
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
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        assertThat(getUriIfAdaptive("https://localhost/echostream")).isNotNull();
        assertThat(getUriIfAdaptive("https://localhost2/echostream2")).isNotNull();
        assertThat(getUriIfAdaptive("https://localhost2:8080/echostream2")).isNotNull();
        assertThat(getUriIfAdaptive("https://localhost/otherstream")).isNull();
        assertThat(getUriIfAdaptive("https://localhost2/otherstream")).isNull();
        assertThat(getUriIfAdaptive("https://otherhost/echostream")).isNull();
        assertThat(getUriIfAdaptive("a QA engineer walks into a bar and orders -1 beer")).isNull();
        assertThat(getUriIfAdaptive("")).isNull();
    }

    // TODO(b/474048542): Move this to CronetAdaptiveRequestContextTest.
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
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        assertThat(getUriIfAdaptive("https://localhost/echostream")).isNull();
        assertThat(getUriIfAdaptive("")).isNull();
        assertThat(getUriIfAdaptive("https://localhost/otherstream")).isNull();
        assertThat(getUriIfAdaptive("https://otherhost/echostream")).isNull();
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Logging is not supported for these implementations.")
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void postViaBidirectionalStreamWithFallbackSet_successOnFallbackNetwork()
            throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // Drop packet for first socket we find.
        mDroppingPacketHandler.mDropFirstRemoteAddress = true;
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

        // The request should succeed because it fell back to the "alternative" network.
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String");
        // Memorize the fallback network.
        assertThat(getFallbackNetworkHandle(url)).isEqualTo(mDefaultNetworkHandle);

        mTestLogger.waitForLogCronetAdaptiveTrafficTerminated();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo()).isNotNull();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo().getWinner())
                .isEqualTo(
                        CronetLogger.CronetAdaptiveTrafficWinner
                                .CRONET_ADAPTIVE_TRAFFIC_WINNER_FALLBACK);
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream"),
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void postViaBidirectionalStreamWithMemorizedFallback_successOnPrimaryNetwork()
            throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        String url = Http2TestServer.getEchoStreamUrl();

        // 1. Manually report a fallback used to memorize it.
        // We use mDefaultNetwork as the memorized fallback.
        long memorizedNetworkHandle = mDefaultNetworkHandle;
        mAdaptiveRequestContext.reportFallbackUsed(url, memorizedNetworkHandle);

        // 2. Now start a stream. It should use memorizedNetworkHandle as PRIMARY.
        // In our setup, the "normal" network handle is DEFAULT_NETWORK_HANDLE
        // (CronetEngineBase.DEFAULT_NETWORK_HANDLE).
        // So they are different, and should be swapped.
        TestBidirectionalStreamCallback callback = new TestBidirectionalStreamCallback();
        callback.addWriteData("Test String".getBytes());

        // Create stream.
        BidirectionalStream stream =
                mCronetEngine
                        .newBidirectionalStreamBuilder(url, callback, callback.getExecutor())
                        .build();
        stream.start();
        callback.blockForDone();

        // The request should succeed on the "primary" (which was the memorized fallback).
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String");

        // Verify that it is still memorized.
        assertThat(getFallbackNetworkHandle(url)).isEqualTo(memorizedNetworkHandle);
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://example.com"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/path"),
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void getFallbackNetworkHandle_networkNotAvailable_returnsNull() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);

        // Setup the mock to return our network.
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});

        mAdaptiveRequestContext.reportFallbackUsed(url, networkHandle);
        assertThat(getFallbackNetworkHandle(url)).isEqualTo(networkHandle);

        // Now mock that the network is NOT in the list of available networks.
        when(mMockConnectivityManagerWrapper.getAllNetworks(any())).thenReturn(new Network[] {});

        // Even if not expired, it should return null because the network is not available.
        assertThat(getFallbackNetworkHandle(url)).isNull();
    }

    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://localhost"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/echostream"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.FAST_IDEMPOTENT_PATHS_FLAG_NAME,
                        value = "/echostream")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Logging is not supported for these implementations.")
    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void postViaBidirectionalStreamWithFastIdempotentRequest_successOnFallbackNetwork()
            throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // Drop packet for first socket AFTER TLS.
        mPostTlsDroppingPacketHandler.mDropFirstRemoteAddress = true;
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

        // The request should succeed because it fell back to the "alternative" network.
        assertThat(stream.isDone()).isTrue();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).isEqualTo("Test String");

        mTestLogger.waitForLogCronetAdaptiveTrafficTerminated();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo()).isNotNull();
        assertThat(mTestLogger.getCronetAdaptiveTrafficTerminatedInfo().getWinner())
                .isEqualTo(
                        CronetLogger.CronetAdaptiveTrafficWinner
                                .CRONET_ADAPTIVE_TRAFFIC_WINNER_FALLBACK);
    }

    private URI getUriIfAdaptive(String url) {
        return mAdaptiveRequestContext.getUriIfAdaptive(url);
    }

    private Long getFallbackNetworkHandle(String url) {
        CronetAdaptiveRequestContext.AdaptiveStreamNetworkHandles handles =
                mAdaptiveRequestContext.computeStreamNetworkHandles(
                        URI.create(url), CronetEngineBase.DEFAULT_NETWORK_HANDLE);
        if (handles == null) {
            return null;
        }
        return handles.mPrimaryNetworkHandle != CronetEngineBase.DEFAULT_NETWORK_HANDLE
                ? handles.mPrimaryNetworkHandle
                : handles.mFallbackNetworkHandle;
    }
}
