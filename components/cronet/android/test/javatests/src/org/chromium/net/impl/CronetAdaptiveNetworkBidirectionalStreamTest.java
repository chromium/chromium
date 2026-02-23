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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.os.Build;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import org.chromium.base.test.util.Batch;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetException;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;
import java.util.concurrent.ScheduledExecutorService;

/** Test functionality of CronetAdaptiveNetworkBidirectionalStream interface. */
@Batch(Batch.PER_CLASS)
@RunWith(AndroidJUnit4.class)
public class CronetAdaptiveNetworkBidirectionalStreamTest {
    private ScheduledExecutorService mMockScheduledExecutorService;
    private BidirectionalStream.Callback mMockCallback;
    private CronetBidirectionalStream mPrimaryStream;
    private CronetBidirectionalStream mFallbackStream;
    private CronetAdaptiveNetworkBidirectionalStream mAdaptiveStream;

    @Before
    public void setUp() throws Exception {
        mMockScheduledExecutorService = mock(ScheduledExecutorService.class);
        mMockCallback = mock(BidirectionalStream.Callback.class);
        mPrimaryStream = mock(CronetBidirectionalStream.class);
        mFallbackStream = mock(CronetBidirectionalStream.class);
        mAdaptiveStream =
                new CronetAdaptiveNetworkBidirectionalStream(
                        mMockCallback, mMockScheduledExecutorService);
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
    public void start_startsPrimaryStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.start();

        verify(mPrimaryStream).start();
        verifyNoInteractions(mMockScheduledExecutorService);
    }

    @Test
    @SmallTest
    public void start_withFallback_schedulesFailover() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.setFallbackStream(mFallbackStream);

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
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        ArgumentCaptor<Runnable> failoverRunnableCaptor = ArgumentCaptor.forClass(Runnable.class);
        mAdaptiveStream.start();
        verify(mMockScheduledExecutorService)
                .schedule(failoverRunnableCaptor.capture(), eq(3000L), eq(MILLISECONDS));

        // Primary becomes ready
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);

        // Trigger failover
        failoverRunnableCaptor.getValue().run();
        verify(mFallbackStream, never()).start();
    }

    @Test
    @SmallTest
    public void onStreamReady_onPrimary_callsCallbackAndDoesNotCancelPrimary() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);

        verify(mMockCallback).onStreamReady(mAdaptiveStream);
        verify(mPrimaryStream, never()).cancel();
    }

    @Test
    @SmallTest
    public void onStreamReady_onFallback_switchesActiveStreamAndCancelsPrimary() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);
        // The primary stream was implicitly cancelled when the fallback stream became ready.
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
    public void onStreamReady_onPrimary_switchesActiveStreamAndCancelsFallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        // The fallback stream was implicitly cancelled when the primary stream became ready.
        verify(mFallbackStream).cancel();

        verify(mMockCallback).onStreamReady(mAdaptiveStream);

        // Verify forwarding now goes to primary stream
        ByteBuffer buffer = ByteBuffer.allocate(100);
        mAdaptiveStream.read(buffer);
        verify(mPrimaryStream).read(buffer);
        verify(mFallbackStream, never()).read(any());
    }

    @Test
    @SmallTest
    public void onResponseHeadersReceived_forwardsToCallback() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
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
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
        mAdaptiveStream.getCallback().onStreamReady(mPrimaryStream);
        mAdaptiveStream.flush();
        verify(mPrimaryStream).flush();
    }

    @Test
    @SmallTest
    public void cancel_forwardsToActiveStream() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);
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
    public void failsWithoutActiveStreamNoFallback_signalsFailed() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);
        mAdaptiveStream.getCallback().onFailed(mPrimaryStream, info, error);
        when(mPrimaryStream.isDone()).thenReturn(true);
        // This is a final failure.
        verify(mMockCallback).onFailed(mAdaptiveStream, info, error);
        assertTrue(mAdaptiveStream.isDone());
    }

    @Test
    @SmallTest
    public void failsWithoutActiveStreamFallbackNotStarted_isNoOp() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        mAdaptiveStream.setPrimaryStream(mPrimaryStream);
        mAdaptiveStream.setFallbackStream(mFallbackStream);

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
        mAdaptiveStream.setFallbackStream(mFallbackStream);

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
        mAdaptiveStream.setFallbackStream(mFallbackStream);
        mAdaptiveStream.getCallback().onStreamReady(mFallbackStream);

        UrlResponseInfo info = mock(UrlResponseInfo.class);
        CronetException error = mock(CronetException.class);
        mAdaptiveStream.getCallback().onFailed(mFallbackStream, info, error);
        when(mFallbackStream.isDone()).thenReturn(true);

        // Active stream failed, so we give up.
        verify(mMockCallback).onFailed(mAdaptiveStream, info, error);
        assertTrue(mAdaptiveStream.isDone());
    }
}
