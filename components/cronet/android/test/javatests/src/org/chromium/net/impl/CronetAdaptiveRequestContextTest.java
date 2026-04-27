// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.net.Network;
import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.ConnectivityManagerWrapper;
import org.chromium.net.CronetTestFramework.CronetImplementation;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.BoolFlag;
import org.chromium.net.CronetTestRule.Flags;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.StringFlag;
import org.chromium.net.httpflags.HttpFlagsLoader;

import java.net.URI;
import java.util.Map;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;

/** Test functionality of CronetAdaptiveRequestContext. */
@DoNotBatch(reason = "HttpFlags are global")
@RunWith(AndroidJUnit4.class)
public class CronetAdaptiveRequestContextTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetAdaptiveRequestContext mContext;
    private FakeClock mFakeClock;
    private ConnectivityManagerWrapper mMockConnectivityManagerWrapper;
    private TestLogger mTestLogger;

    private static class FakeClock extends CronetAdaptiveRequestContext.Clock {
        private long mElapsedRealtime;

        @Override
        long elapsedRealtime() {
            return mElapsedRealtime;
        }

        void advanceTime(long millis) {
            mElapsedRealtime += millis;
        }
    }

    @Before
    public void setUp() throws Exception {
        HttpFlagsLoader.flushHttpFlags();
        Context context = mTestRule.getTestFramework().getContext();
        mFakeClock = new FakeClock();
        mTestLogger = new TestLogger();
        mContext = new CronetAdaptiveRequestContext(context, mTestLogger, mFakeClock);
        mMockConnectivityManagerWrapper = mock(ConnectivityManagerWrapper.class);
        mContext.setConnectivityManagerWrapperForTest(mMockConnectivityManagerWrapper);
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://example.com"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/path,/other")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void reportFallbackUsed_memorizesNetwork() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(mockNetwork);

        mContext.reportFallbackUsed(URI.create(url), networkHandle);

        assertEquals(networkHandle, computeStreamNetworkHandles(url).mPrimaryNetworkHandle);
        assertEquals(
                networkHandle,
                computeStreamNetworkHandles("https://example.com/other").mPrimaryNetworkHandle);
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://example.com"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/path,/other")
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
    public void telemetrySmokeTest() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(mockNetwork);

        mContext.reportFallbackUsed(URI.create(url), networkHandle);

        assertEquals(networkHandle, computeStreamNetworkHandles(url).mPrimaryNetworkHandle);
        assertEquals(
                networkHandle,
                computeStreamNetworkHandles("https://example.com/other").mPrimaryNetworkHandle);

        mTestLogger.waitForLogCronetAdaptiveTrafficAlternateNetworkComputation();
        assertTrue(mTestLogger.getFallbackNetworkCacheHit());
        assertEquals(1, mTestLogger.getNumberOfAvailableNetworks());
        assertTrue(mTestLogger.getDefaultNetworkIsKnown());
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://example.com"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/path")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void getFallbackNetwork_expired_returnsNull() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(mockNetwork);

        mContext.reportFallbackUsed(URI.create(url), networkHandle);
        assertEquals(networkHandle, computeStreamNetworkHandles(url).mPrimaryNetworkHandle);

        // Advance time just past the 10s expiration.
        mFakeClock.advanceTime(10001);

        assertEquals(null, computeStreamNetworkHandles(url));
    }

    @Test
    @SmallTest
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
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void getFallbackNetwork_notExpired_returnsNetwork() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(mockNetwork);

        mContext.reportFallbackUsed(URI.create(url), networkHandle);

        // Advance time almost to expiration.
        mFakeClock.advanceTime(9999);

        assertEquals(networkHandle, computeStreamNetworkHandles(url).mPrimaryNetworkHandle);
    }

    @Test
    @SmallTest
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
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void getFallbackNetwork_networkNotAvailable_returnsNull() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;

        // Mock the network is NOT in the list of available networks.
        when(mMockConnectivityManagerWrapper.getAllNetworks(any())).thenReturn(new Network[] {});

        mContext.reportFallbackUsed(URI.create(url), networkHandle);

        // Even if not expired, it should return null because the network is not available.
        assertEquals(null, computeStreamNetworkHandles(url));
    }

    @Test
    @SmallTest
    @Flags(
            stringFlags = {
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME,
                        value = "https://example.com"),
                @StringFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME,
                        value = "/path")
            },
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_NAME,
                        value = true)
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void reportFallbackUsed_defaultNetwork_clearsMemory() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        long networkHandle = 12345L;
        Network mockNetwork = mock(Network.class);
        when(mockNetwork.getNetworkHandle()).thenReturn(networkHandle);
        when(mMockConnectivityManagerWrapper.getAllNetworks(any()))
                .thenReturn(new Network[] {mockNetwork});
        when(mMockConnectivityManagerWrapper.getDefaultNetwork()).thenReturn(mockNetwork);

        // First memorize a fallback.
        mContext.reportFallbackUsed(URI.create(url), networkHandle);
        assertEquals(networkHandle, computeStreamNetworkHandles(url).mPrimaryNetworkHandle);

        // Now report default network.
        mContext.reportFallbackUsed(URI.create(url), CronetEngineBase.DEFAULT_NETWORK_HANDLE);

        // Memory should be cleared.
        assertEquals(null, computeStreamNetworkHandles(url));
    }

    @Test
    @SmallTest
    @Flags(
            boolFlags = {
                @BoolFlag(
                        name = CronetAdaptiveRequestContext.ENABLE_ADAPTIVE_NETWORK_FOR_ALL_NAME,
                        value = true)
            })
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void getUriIfAdaptive_allEnabled_returnsUri() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://random-host.com/random-path";
        URI expectedUri = URI.create(url);
        assertEquals(expectedUri, mContext.getUriIfAdaptive(url));
    }

    @Test
    @SmallTest
    public void reportFallbackUsed_notifiesActiveStreams() {
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        ScheduledExecutorService mockExecutor1 = mock(ScheduledExecutorService.class);
        ScheduledExecutorService mockExecutor2 = mock(ScheduledExecutorService.class);
        ScheduledFuture<?> mockFuture1 = mock(ScheduledFuture.class);
        ScheduledFuture<?> mockFuture2 = mock(ScheduledFuture.class);

        when(mockFuture1.cancel(false)).thenReturn(true);
        when(mockFuture2.cancel(false)).thenReturn(true);

        doReturn(mockFuture1).when(mockExecutor1).schedule(any(Runnable.class), anyLong(), any());
        doReturn(mockFuture2).when(mockExecutor2).schedule(any(Runnable.class), anyLong(), any());

        CronetAdaptiveNetworkBidirectionalStream stream1 =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mock(BidirectionalStream.Callback.class),
                        mockExecutor1,
                        mContext,
                        URI.create("https://example.com/path"),
                        mock(CronetLogger.class),
                        false);

        CronetAdaptiveNetworkBidirectionalStream stream2 =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mock(BidirectionalStream.Callback.class),
                        mockExecutor2,
                        mContext,
                        URI.create("https://example.com/path"),
                        mock(CronetLogger.class),
                        false);

        CronetBidirectionalStream mockPrimary1 = mock(CronetBidirectionalStream.class);
        CronetBidirectionalStream mockFallback1 = mock(CronetBidirectionalStream.class);
        stream1.setPrimaryStream(mockPrimary1);
        stream1.setFallbackStream(mockFallback1);

        CronetBidirectionalStream mockPrimary2 = mock(CronetBidirectionalStream.class);
        CronetBidirectionalStream mockFallback2 = mock(CronetBidirectionalStream.class);
        stream2.setPrimaryStream(mockPrimary2);
        stream2.setFallbackStream(mockFallback2);

        long fallbackNetworkHandle = 12345L;
        when(mockFallback1.getTargetNetworkHandle()).thenReturn(fallbackNetworkHandle);
        when(mockFallback2.getTargetNetworkHandle()).thenReturn(fallbackNetworkHandle);

        // Call start() to register and set future
        stream1.start();
        stream2.start();

        String url = "https://example.com/path";
        mContext.reportFallbackUsed(URI.create(url), fallbackNetworkHandle);

        // Verify that both fallback streams were started (proving notification was delivered)
        verify(mockFallback1).start();
        verify(mockFallback2).start();
    }

    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void reportFallbackUsed_toasts() {
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        try (var overrides =
                new AndroidOsSystemProperties.WithOverridesForTesting(
                        Map.of(
                                CronetAdaptiveRequestContext
                                        .ADAPTIVE_NETWORK_DEV_TOAST_PROPERTY_NAME,
                                "true"))) {

            Context context = mTestRule.getTestFramework().getContext();
            CronetAdaptiveRequestContext contextForTest =
                    new CronetAdaptiveRequestContext(context, mTestLogger, mFakeClock);
            contextForTest.setConnectivityManagerWrapperForTest(mMockConnectivityManagerWrapper);

            CronetAdaptiveRequestContext spyContext = spy(contextForTest);
            String url = "https://example.com/path";

            spyContext.reportFallbackUsed(URI.create(url), 12345L);
            verify(spyContext).showDevToast("CRONET: Fallback used example.com/path def: N");

            spyContext.reportFallbackUsed(URI.create(url), CronetEngineBase.DEFAULT_NETWORK_HANDLE);
            verify(spyContext).showDevToast("CRONET: Fallback used example.com/path def: Y");
        }
    }

    @Test
    @SmallTest
    @RequiresMinAndroidApi(Build.VERSION_CODES.N)
    public void maybeShowInitialDevToast_toasts() throws Exception {
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        try (var overrides =
                new AndroidOsSystemProperties.WithOverridesForTesting(
                        Map.of(
                                CronetAdaptiveRequestContext
                                        .ADAPTIVE_NETWORK_DEV_TOAST_PROPERTY_NAME,
                                "true"))) {

            Context context = mTestRule.getTestFramework().getContext();
            CronetAdaptiveRequestContext contextForTest =
                    new CronetAdaptiveRequestContext(context, mTestLogger, mFakeClock);
            contextForTest.setConnectivityManagerWrapperForTest(mMockConnectivityManagerWrapper);

            // Reset sToastShown to trigger it again via spy.
            CronetAdaptiveRequestContext.sToastShown = false;

            CronetAdaptiveRequestContext spyContext = spy(contextForTest);
            spyContext.maybeShowInitialDevToast();

            verify(spyContext)
                    .showDevToast("CRONET[org.chromium.net.tests]: CANS enabled: N, for all: N");
        }
    }

    @Test
    @SmallTest
    public void registerStream_calledTwice_throwsAssertionError() {
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // Create real stream to avoid mocking final class
        CronetAdaptiveNetworkBidirectionalStream stream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mock(BidirectionalStream.Callback.class),
                        mock(ScheduledExecutorService.class),
                        mContext,
                        URI.create("https://example.com/path"),
                        mock(CronetLogger.class),
                        false);

        mContext.registerStream(stream);

        assertThrows(AssertionError.class, () -> mContext.registerStream(stream));
    }

    @Test
    @SmallTest
    public void unregisterStream_notRegistered_throwsAssertionError() {
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        // Create real stream to avoid mocking final class
        CronetAdaptiveNetworkBidirectionalStream stream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mock(BidirectionalStream.Callback.class),
                        mock(ScheduledExecutorService.class),
                        mContext,
                        URI.create("https://example.com/path"),
                        mock(CronetLogger.class),
                        false);

        assertThrows(AssertionError.class, () -> mContext.unregisterStream(stream));
    }

    private CronetAdaptiveRequestContext.AdaptiveStreamNetworkHandles computeStreamNetworkHandles(
            String url) {
        return mContext.computeStreamNetworkHandles(
                URI.create(url), CronetEngineBase.DEFAULT_NETWORK_HANDLE);
    }
}
