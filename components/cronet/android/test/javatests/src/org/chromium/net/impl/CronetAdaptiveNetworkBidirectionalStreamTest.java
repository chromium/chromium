// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.net.Network;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import org.chromium.base.test.util.Batch;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.ConnectivityManagerWrapper;
import org.chromium.net.CronetException;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;

/** Test functionality of CronetAdaptiveNetworkBidirectionalStream interface. */
@Batch(Batch.PER_CLASS)
@RunWith(AndroidJUnit4.class)
public class CronetAdaptiveNetworkBidirectionalStreamTest {
    private ScheduledExecutorService mMockScheduledExecutorService;
    private BidirectionalStream.Callback mMockCallback;
    private CronetBidirectionalStream mPrimaryStream;
    private CronetBidirectionalStream mFallbackStream;
    private CronetAdaptiveNetworkBidirectionalStream mAdaptiveStream;
    private CronetAdaptiveRequestContext mMockAdaptiveRequestContext;
    private static final String TEST_URL = "https://example.com/path";
    private TestLogger mTestLogger;

    @Before
    public void setUp() throws Exception {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mMockScheduledExecutorService = mock(ScheduledExecutorService.class);
        when(mMockScheduledExecutorService.schedule(
                        any(Runnable.class),
                        any(Long.class),
                        any(java.util.concurrent.TimeUnit.class)))
                .thenReturn(mock(ScheduledFuture.class));
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mMockScheduledExecutorService)
                .execute(any(Runnable.class));
        mMockCallback = mock(BidirectionalStream.Callback.class);
        mPrimaryStream = mock(CronetBidirectionalStream.class);
        mFallbackStream = mock(CronetBidirectionalStream.class);
        when(mPrimaryStream.getTargetNetworkHandle())
                .thenReturn(CronetEngineBase.DEFAULT_NETWORK_HANDLE);
        when(mFallbackStream.getTargetNetworkHandle())
                .thenReturn(CronetEngineBase.DEFAULT_NETWORK_HANDLE);
        mTestLogger = new TestLogger();
        mMockAdaptiveRequestContext = mock(CronetAdaptiveRequestContext.class);
        when(mMockAdaptiveRequestContext.getReadyFailoverMs()).thenReturn(3000L);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ false);
        mAdaptiveStream.setFallbackStream(mFallbackStream);
    }

    @Test
    @SmallTest
    public void missingPrimaryStream_throwsException() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        assertThrows(NullPointerException.class, () -> mAdaptiveStream.start());
    }

    @Test
    @SmallTest
    public void start_startsPrimaryStreamAndSchedulesFailover() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();

        verify(mPrimaryStream).start();
        verify(mMockScheduledExecutorService)
                .schedule(any(Runnable.class), eq(3000L), eq(MILLISECONDS));
    }

    @Test
    @SmallTest
    public void failover_startsFallbackStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        ArgumentCaptor<Runnable> failoverRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        mAdaptiveStream.start();
        verify(mMockScheduledExecutorService)
                .schedule(failoverRunnableCaptor.capture(), eq(3000L), eq(MILLISECONDS));

        // Trigger failover
        failoverRunnableCaptor.getValue().run();
        verify(mFallbackStream).start();
    }

    @Test
    @SmallTest
    public void failover_afterPrimaryReady_doesNotStartFallbackStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        ArgumentCaptor<Runnable> failoverRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        mAdaptiveStream.start();
        verify(mMockScheduledExecutorService)
                .schedule(failoverRunnableCaptor.capture(), eq(3000L), eq(MILLISECONDS));

        // Primary becomes ready
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);

        // Failover is canceled when primary becomes ready, so we shouldn't trigger it.
        verify(mFallbackStream, never()).start();
    }

    @Test
    @SmallTest
    public void onStreamReady_onPrimary_callsCallbackAndCancelsFallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);

        verify(mMockCallback).onStreamReady(mAdaptiveStream);
        verify(mFallbackStream).cancel();
    }

    @Test
    @SmallTest
    public void onStreamReady_onFallback_switchesActiveStreamAndCancelsPrimary() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);
        // The primary stream was implicitly canceled when the fallback stream became ready.
        verify(mPrimaryStream).cancel();

        verify(mMockCallback).onStreamReady(mAdaptiveStream);

        // Verify forwarding now goes to fallback stream
        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.read(buffer);
        verify(mFallbackStream).read(buffer);
        verify(mPrimaryStream, never()).read(any());
    }

    @Test
    @SmallTest
    public void onStreamReady_onFallback_reportsFallbackUsed() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        long networkHandle = 123456789L;
        when(mFallbackStream.getTargetNetworkHandle()).thenReturn(networkHandle);

        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        verify(mMockAdaptiveRequestContext).reportFallbackUsed(eq(TEST_URL), eq(networkHandle));
    }

    @Test
    @SmallTest
    public void onResponseHeadersReceived_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onResponseHeadersReceived(mPrimaryStream, info);

        verify(mMockCallback).onResponseHeadersReceived(mAdaptiveStream, info);
    }

    @Test
    @SmallTest
    public void onReadCompleted_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        ByteBuffer buffer = ByteBuffer.allocate(100);

        mAdaptiveStream.getCallback().onReadCompleted(mPrimaryStream, info, buffer, true);

        verify(mMockCallback).onReadCompleted(mAdaptiveStream, info, buffer, true);
    }

    @Test
    @SmallTest
    public void onWriteCompleted_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        ByteBuffer buffer = ByteBuffer.allocate(100);

        mAdaptiveStream.getCallback().onWriteCompleted(mPrimaryStream, info, buffer, false);

        verify(mMockCallback).onWriteCompleted(mAdaptiveStream, info, buffer, false);
    }

    @Test
    @SmallTest
    public void onResponseTrailersReceived_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        UrlResponseInfo.HeaderBlock trailers = mock(UrlResponseInfo.HeaderBlock.class);

        mAdaptiveStream.getCallback().onResponseTrailersReceived(mPrimaryStream, info, trailers);

        verify(mMockCallback).onResponseTrailersReceived(mAdaptiveStream, info, trailers);
    }

    @Test
    @SmallTest
    public void onSucceeded_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onSucceeded(mPrimaryStream, info);

        verify(mMockCallback).onSucceeded(mAdaptiveStream, info);
    }

    @Test
    @SmallTest
    public void onFailed_forwardsToCallbackForActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);

        mAdaptiveStream.getCallback().onFailed(mPrimaryStream, info, error);
        when(mPrimaryStream.isDone()).thenReturn(true);

        verify(mMockCallback).onFailed(mAdaptiveStream, info, error);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void onFailed_ignoresInactiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);

        mAdaptiveStream.getCallback().onFailed(mPrimaryStream, info, error);

        verify(mMockCallback, never()).onFailed(any(), any(), any());
    }

    @Test
    @SmallTest
    public void onCanceled_forwardsToCallbackForActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onCanceled(mPrimaryStream, info);
        when(mPrimaryStream.isDone()).thenReturn(true);

        verify(mMockCallback).onCanceled(mAdaptiveStream, info);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void onCanceledPrimaryOnly_noop() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onCanceled(mPrimaryStream, info);
        when(mPrimaryStream.isDone()).thenReturn(true);

        verify(mMockCallback, never()).onCanceled(any(), any());
        assertFalse(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void onCanceledBothStreams_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onCanceled(mFallbackStream, info);
        when(mFallbackStream.isDone()).thenReturn(true);
        mAdaptiveStream.getCallback().onCanceled(mPrimaryStream, info);
        when(mPrimaryStream.isDone()).thenReturn(true);

        verify(mMockCallback).onCanceled(mAdaptiveStream, info);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void onCanceled_ignoresInactiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onCanceled(mFallbackStream, info);
        when(mFallbackStream.isDone()).thenReturn(true);

        verify(mMockCallback, never()).onCanceled(any(), any());
        assertFalse(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void read_forwardsToActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.read(buffer);
        verify(mPrimaryStream).read(buffer);
    }

    @Test
    @SmallTest
    public void write_forwardsToActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, true);
        verify(mPrimaryStream).write(buffer, true);
    }

    @Test
    @SmallTest
    public void flush_forwardsToActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        mAdaptiveStream.flush();
        verify(mPrimaryStream).flush();
    }

    @Test
    @SmallTest
    public void cancel_forwardsToBothStreams() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.cancel();
        verify(mPrimaryStream).cancel();
        when(mPrimaryStream.isDone()).thenReturn(true);
        verify(mFallbackStream).cancel();
        when(mFallbackStream.isDone()).thenReturn(true);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void isDone_forwardsToActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        when(mPrimaryStream.isDone()).thenReturn(true);
        assertEquals(true, mAdaptiveStream.isDone());
        verify(mPrimaryStream).isDone();
    }

    @Test
    @SmallTest
    public void isDone_withoutActiveStream_returnsFalse() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        assertEquals(false, mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void failsWithoutActiveStreamFallbackNotStarted_isNoOp() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);
        mAdaptiveStream.getCallback().onFailed(mPrimaryStream, info, error);

        // The fallback stream still hasn't failed, so we don't give up yet.
        verify(mMockCallback, never()).onFailed(any(), any(), any());
    }

    @Test
    @SmallTest
    public void bothStreamsFailed_signalsFailed() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);
        mAdaptiveStream.getCallback().onFailed(mPrimaryStream, info, error);
        when(mPrimaryStream.isDone()).thenReturn(true);
        mAdaptiveStream.getCallback().onFailed(mFallbackStream, info, error);
        when(mFallbackStream.isDone()).thenReturn(true);

        // Both failed, so now we give up.
        verify(mMockCallback).onFailed(mAdaptiveStream, info, error);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void failsActiveStream_signalsFailed() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);
        mAdaptiveStream.getCallback().onFailed(mFallbackStream, info, error);
        when(mFallbackStream.isDone()).thenReturn(true);

        // Active stream failed, so we give up.
        verify(mMockCallback).onFailed(mAdaptiveStream, info, error);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void testComputeAlternativeNetwork_noNetworks_returnsNull() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        ConnectivityManagerWrapper mockConnectivityManagerWrapper =
                mock(ConnectivityManagerWrapper.class);
        when(mockConnectivityManagerWrapper.getAllNetworks(null)).thenReturn(new Network[0]);

        CronetAdaptiveRequestContext adaptiveRequestContext =
                new CronetAdaptiveRequestContext(
                        ApplicationProvider.getApplicationContext(), mTestLogger);
        adaptiveRequestContext.setConnectivityManagerWrapperForTest(mockConnectivityManagerWrapper);

        assertEquals(
                null,
                adaptiveRequestContext.computeAlternativeNetworkHandle(
                        adaptiveRequestContext.getAllNetworks(), null));
    }

    @Test
    @SmallTest
    public void testComputeAlternativeNetwork_onlyDefaultNetwork_returnsNull() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        ConnectivityManagerWrapper mockConnectivityManagerWrapper =
                mock(ConnectivityManagerWrapper.class);
        Network defaultNetwork = mock(Network.class);
        when(mockConnectivityManagerWrapper.getAllNetworks(null))
                .thenReturn(new Network[] {defaultNetwork});

        CronetAdaptiveRequestContext adaptiveRequestContext =
                new CronetAdaptiveRequestContext(
                        ApplicationProvider.getApplicationContext(), mTestLogger);
        adaptiveRequestContext.setConnectivityManagerWrapperForTest(mockConnectivityManagerWrapper);

        assertEquals(
                null,
                adaptiveRequestContext.computeAlternativeNetworkHandle(
                        adaptiveRequestContext.getAllNetworks(), defaultNetwork));
    }

    @Test
    @SmallTest
    public void testComputeAlternativeNetwork_alternativeNetworkAvailable_returnsAlternative() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        ConnectivityManagerWrapper mockConnectivityManagerWrapper =
                mock(ConnectivityManagerWrapper.class);
        Network defaultNetwork = mock(Network.class);
        Network alternativeNetwork = mock(Network.class);
        long alternativeHandle = 987654321L;
        when(alternativeNetwork.getNetworkHandle()).thenReturn(alternativeHandle);

        when(mockConnectivityManagerWrapper.getAllNetworks(null))
                .thenReturn(new Network[] {defaultNetwork, alternativeNetwork});

        CronetAdaptiveRequestContext adaptiveRequestContext =
                new CronetAdaptiveRequestContext(
                        ApplicationProvider.getApplicationContext(), mTestLogger);
        adaptiveRequestContext.setConnectivityManagerWrapperForTest(mockConnectivityManagerWrapper);

        assertEquals(
                alternativeHandle,
                (long)
                        adaptiveRequestContext.computeAlternativeNetworkHandle(
                                adaptiveRequestContext.getAllNetworks(), defaultNetwork));
    }

    @Test
    @SmallTest
    public void cancel_beforeFailoverRuns_cancelsFutureAndDoesNotStartFallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        ArgumentCaptor<Runnable> failoverRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        ScheduledFuture mockFuture = mock(ScheduledFuture.class);
        when(mMockScheduledExecutorService.schedule(
                        any(Runnable.class), eq(3000L), eq(MILLISECONDS)))
                .thenReturn(mockFuture);

        mAdaptiveStream.start();
        verify(mMockScheduledExecutorService)
                .schedule(failoverRunnableCaptor.capture(), eq(3000L), eq(MILLISECONDS));

        // Cancel the adaptive stream
        CronetBidirectionalStream primary = mAdaptiveStream.mPrimaryStream;
        CronetBidirectionalStream fallback = mAdaptiveStream.mFallbackStream;

        mAdaptiveStream.cancel();

        verify(primary).cancel();
        verify(fallback).cancel();
        verify(mockFuture).cancel(false);

        // Failover is canceled when adaptive stream is canceled, so we shouldn't trigger it.
        verify(mFallbackStream, never()).start();
    }

    @Test
    @SmallTest
    public void onStreamReady_beforeFailoverRuns_cancelsFutureAndDoesNotStartFallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        ArgumentCaptor<Runnable> failoverRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        ScheduledFuture mockFuture = mock(ScheduledFuture.class);
        when(mMockScheduledExecutorService.schedule(
                        any(Runnable.class), eq(3000L), eq(MILLISECONDS)))
                .thenReturn(mockFuture);

        mAdaptiveStream.start();
        verify(mMockScheduledExecutorService)
                .schedule(failoverRunnableCaptor.capture(), eq(3000L), eq(MILLISECONDS));

        // Primary becomes ready
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        verify(mockFuture).cancel(false);

        // Failover is canceled when primary becomes ready, so we shouldn't trigger it.
        verify(mFallbackStream, never()).start();
    }

    @Test
    @SmallTest
    public void cancel_whenFutureCannotBeCanceled_schedulesFallbackCancel() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        ScheduledFuture mockFuture = mock(ScheduledFuture.class);
        when(mMockScheduledExecutorService.schedule(
                        any(Runnable.class), eq(3000L), eq(MILLISECONDS)))
                .thenReturn(mockFuture);
        // Simulate that the future cannot be canceled (e.g., it's already running).
        when(mockFuture.cancel(false)).thenReturn(false);

        mAdaptiveStream.start();

        // Cancel the adaptive stream
        mAdaptiveStream.cancel();

        verify(mockFuture).cancel(false);
        // verify(mMockScheduledExecutorService).execute(any(Runnable.class)) is implicit because
        // the setUp() mock calls run() immediately.
        verify(mFallbackStream).cancel();
    }

    @Test
    @SmallTest
    public void testFastIdempotent_writeBeforeReady_buffers() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, false);

        verify(mPrimaryStream, never()).write(any(), any(Boolean.class));
        verify(mFallbackStream, never()).write(any(), any(Boolean.class));
    }

    @Test
    @SmallTest
    public void testFastIdempotent_onStreamReady_replaysWrites() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ByteBuffer buffer = ByteBuffer.allocate(100);
        buffer.put((byte) 1);
        buffer.flip();
        mAdaptiveStream.write(buffer, false);

        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);

        // Replay should happen. The replayed buffer should have same content.
        ArgumentCaptor<ByteBuffer> bufferCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
        verify(mPrimaryStream).write(bufferCaptor.capture(), eq(false));
        ByteBuffer replayedBuffer = bufferCaptor.getValue();
        assertEquals(1, replayedBuffer.remaining());
        assertEquals((byte) 1, replayedBuffer.get());

        verify(mPrimaryStream).flush();
        verify(mMockCallback).onStreamReady(mAdaptiveStream);
    }

    @Test
    @SmallTest
    public void testFastIdempotent_onBothStreamsReady_replaysToBoth() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, false);

        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        verify(mPrimaryStream).write(any(ByteBuffer.class), eq(false));
        verify(mFallbackStream).write(any(ByteBuffer.class), eq(false));
        verify(mMockCallback).onStreamReady(mAdaptiveStream);
    }

    @Test
    @SmallTest
    public void testFastIdempotent_onResponseHeaders_setsActiveStreamAndCancelsOther() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);
        mAdaptiveStream.start();

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        // Fallback responds first
        mAdaptiveStream.getCallback().onResponseHeadersReceived(mFallbackStream, info);

        verify(mPrimaryStream).cancel();
        verify(mMockCallback).onResponseHeadersReceived(mAdaptiveStream, info);

        // Further writes should only go to fallback
        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, false);
        verify(mFallbackStream).write(buffer, false);
        verify(mPrimaryStream, never()).write(buffer, false);
    }

    @Test
    @SmallTest
    public void testFastIdempotent_onWriteCompleted_forwardsOriginal() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, false);

        // Fallback becomes ready and replays
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        ArgumentCaptor<ByteBuffer> bufferCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
        verify(mFallbackStream).write(bufferCaptor.capture(), eq(false));
        ByteBuffer replayedBuffer = bufferCaptor.getValue();

        // Headers received on fallback, so it becomes active
        UrlResponseInfo info = mock(UrlResponseInfo.class);
        mAdaptiveStream.getCallback().onResponseHeadersReceived(mFallbackStream, info);

        // Write completion for replayed buffer on fallback
        mAdaptiveStream
                .getCallback()
                .onWriteCompleted(mFallbackStream, info, replayedBuffer, false);

        // Should be forwarded to callback with the ORIGINAL buffer.
        verify(mMockCallback).onWriteCompleted(mAdaptiveStream, info, buffer, false);
        assertEquals(buffer.limit(), buffer.position());
    }

    @Test
    @SmallTest
    public void testFastIdempotent_onWriteCompleted_reportsOnlyOnce() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback,
                        mMockScheduledExecutorService,
                        mMockAdaptiveRequestContext,
                        TEST_URL,
                        mTestLogger,
                        /* isFastIdempotentRequest= */ true);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.write(buffer, false);

        // Both become ready and replay
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        ArgumentCaptor<ByteBuffer> primaryBufferCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
        verify(mPrimaryStream).write(primaryBufferCaptor.capture(), eq(false));
        ByteBuffer primaryReplayedBuffer = primaryBufferCaptor.getValue();

        ArgumentCaptor<ByteBuffer> fallbackBufferCaptor = ArgumentCaptor.forClass(ByteBuffer.class);
        verify(mFallbackStream).write(fallbackBufferCaptor.capture(), eq(false));
        ByteBuffer fallbackReplayedBuffer = fallbackBufferCaptor.getValue();

        UrlResponseInfo info = mock(UrlResponseInfo.class);

        // 1. Completion on primary
        mAdaptiveStream
                .getCallback()
                .onWriteCompleted(mPrimaryStream, info, primaryReplayedBuffer, false);
        verify(mMockCallback).onWriteCompleted(mAdaptiveStream, info, buffer, false);

        // 2. Completion on fallback
        mAdaptiveStream
                .getCallback()
                .onWriteCompleted(mFallbackStream, info, fallbackReplayedBuffer, false);
        // Should NOT be reported again. Total calls should still be 1.
        verify(mMockCallback, times(1))
                .onWriteCompleted(any(), any(), eq(buffer), any(Boolean.class));
    }

    @Test
    @SmallTest
    public void onSucceeded_onFallback_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);
        UrlResponseInfo info = mock(UrlResponseInfo.class);

        mAdaptiveStream.getCallback().onSucceeded(mFallbackStream, info);

        verify(mMockCallback).onSucceeded(mAdaptiveStream, info);
    }

    @Test
    @SmallTest
    public void isDone_withoutActiveStream_returnsTrueIfBothDone() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        when(mPrimaryStream.isDone()).thenReturn(true);
        when(mFallbackStream.isDone()).thenReturn(true);
        assertTrue(mAdaptiveStream.isDone());
    }
}
