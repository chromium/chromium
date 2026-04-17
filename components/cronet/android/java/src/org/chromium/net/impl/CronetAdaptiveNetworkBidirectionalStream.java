// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import androidx.annotation.VisibleForTesting;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetException;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import javax.annotation.concurrent.GuardedBy;

/**
 * A {@link BidirectionalStream} implementation that allows fallback to an alternative stream if the
 * primary stream is not ready within a certain timeout.
 */
final class CronetAdaptiveNetworkBidirectionalStream extends ExperimentalBidirectionalStream {
    private final Object mLock = new Object();
    @VisibleForTesting CronetBidirectionalStream mPrimaryStream;
    // Used to ensure that we always report a terminal callback to mBackendCallback. This is
    // necessary to cover the scenario where both underlying streams terminate without ever becoming
    // active.
    private final AtomicBoolean mOnlyOneStreamRemains;
    @VisibleForTesting CronetBidirectionalStream mFallbackStream;

    private final ScheduledExecutorService mExecutor;
    private final AtomicReference<BidirectionalStream> mActiveStream = new AtomicReference<>(null);

    private final AtomicReference<ScheduledFuture<?>> mFailoverFuture = new AtomicReference<>();

    /** The developer facing callback. */
    private final BidirectionalStream.Callback mBackendCallback;

    private final CronetAdaptiveRequestContext mAdaptiveRequestContext;
    private final CronetLogger mLogger;
    private final String mUrl;

    private final CronetLogger.CronetAdaptiveTrafficTerminatedInfo mLoggingState =
            new CronetLogger.CronetAdaptiveTrafficTerminatedInfo(
                    NativeCronetEngineBuilderImpl.getCronetSource());

    // If we can send the request payload twice.
    // The "fast" implies that the request should be expected to complete within a couple of seconds
    // at most.
    private final boolean mIsFastIdempotentRequest;

    /** Writes that happened before any stream was ready. */
    @GuardedBy("mLock")
    private final List<PendingWrite> mPendingWrites = new ArrayList<>();

    /**
     * Tracks buffers that are being re-sent to multiple streams to avoid duplicate onWriteCompleted
     * callbacks.
     */
    @GuardedBy("mLock")
    private final Set<ByteBuffer> mReplayedBuffers =
            Collections.newSetFromMap(new IdentityHashMap<>());

    private final AtomicBoolean mCallbackOnStreamReadyCalled = new AtomicBoolean(false);

    /** Underlying streams that are ready to receive writes in fast idempotent mode. */
    @GuardedBy("mLock")
    private final Set<BidirectionalStream> mReadyStreams =
            Collections.newSetFromMap(new IdentityHashMap<>());

    private static class PendingWrite {
        final ByteBuffer mBuffer;
        final boolean mEndOfStream;

        PendingWrite(ByteBuffer buffer, boolean endOfStream) {
            this.mBuffer = buffer;
            this.mEndOfStream = endOfStream;
        }
    }

    /**
     * The callback passed to both streams - picking the winner and calling mBackendCallback.
     *
     * <p>This is either a RaceUntilOnResponseHeadersReceivedCallback or a
     * RaceUntilOnStreamReadyCallback which distinct behaviors towards mBackendCallback defined in
     * those subclasses.
     */
    private final BidirectionalStream.Callback mRedirectingCallback;

    private class RaceUntilOnStreamReadyCallback extends BidirectionalStream.Callback {
        /**
         * This callback happens on both the primary and fallback stream. But the stream being ready
         * first "wins" and will be used for future operations.
         */
        @Override
        public void onStreamReady(BidirectionalStream stream) {
            checkValidStream(stream);
            if (mActiveStream.compareAndSet(null, stream)) {
                setActiveStreamWinner(stream);
                mBackendCallback.onStreamReady(CronetAdaptiveNetworkBidirectionalStream.this);
            }
        }

        @Override
        public void onResponseHeadersReceived(BidirectionalStream stream, UrlResponseInfo info) {
            checkValidStream(stream);
            if (mActiveStream.get() == stream) {
                mBackendCallback.onResponseHeadersReceived(
                        CronetAdaptiveNetworkBidirectionalStream.this, info);
            }
        }

        @Override
        public void onReadCompleted(
                BidirectionalStream stream,
                UrlResponseInfo info,
                ByteBuffer buffer,
                boolean endOfStream) {
            checkValidStream(stream);
            if (mActiveStream.get() == stream) {
                mBackendCallback.onReadCompleted(
                        CronetAdaptiveNetworkBidirectionalStream.this, info, buffer, endOfStream);
            }
        }

        @Override
        public void onWriteCompleted(
                BidirectionalStream stream,
                UrlResponseInfo info,
                ByteBuffer buffer,
                boolean endOfStream) {
            checkValidStream(stream);
            if (mActiveStream.get() == stream) {
                mBackendCallback.onWriteCompleted(
                        CronetAdaptiveNetworkBidirectionalStream.this, info, buffer, endOfStream);
            }
        }

        @Override
        public void onResponseTrailersReceived(
                BidirectionalStream stream,
                UrlResponseInfo info,
                UrlResponseInfo.HeaderBlock trailers) {
            checkValidStream(stream);
            if (mActiveStream.get() == stream) {
                mBackendCallback.onResponseTrailersReceived(
                        CronetAdaptiveNetworkBidirectionalStream.this, info, trailers);
            }
        }

        @Override
        public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
            checkValidStream(stream);

            if (stream == mPrimaryStream) {
                mLoggingState.setMainRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_SUCCEEDED);
            } else {
                mLoggingState.setFallbackRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_SUCCEEDED);
            }

            // It is impossible for an underlying stream to reach onSucceeded, without first
            // going through onStreamReady. Therefore, it is impossible for the non-active
            // stream to reach onSucceeded and mOnlyOneStreamRemains to be true at the same
            // time. This would require the active stream to have reached a terminal state,
            // in which case we would have already reported a terminal callback.
            //
            // This means that here, differently from onFailed and onCanceled, we can only
            // be in the "the active stream succeeded" scenario. In which case, we can
            // forward to the backend callback and we do not need to worry about the other
            // stream.

            if (mActiveStream.get() == stream) {
                mBackendCallback.onSucceeded(CronetAdaptiveNetworkBidirectionalStream.this, info);
                mLogger.logCronetAdaptiveTrafficTerminated(mLoggingState);
            }
        }

        @Override
        public void onFailed(
                BidirectionalStream stream, UrlResponseInfo info, CronetException error) {
            checkValidStream(stream);

            if (stream == mPrimaryStream) {
                mLoggingState.setMainRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_FAILED);
            } else {
                mLoggingState.setFallbackRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_FAILED);
            }

            // We are in one of two scenarios:
            // 1. The active stream failed. In this case we forward to the backend
            //    callback, without having to worry about the other stream.
            // 2. The non-active stream failed. In this case we need to worry about
            //    whether the other stream ever became active. This is handled via
            //    mOnlyOneStreamRemains, which is only set by non-active streams.
            if (mActiveStream.get() == stream || mOnlyOneStreamRemains.getAndSet(true)) {
                mBackendCallback.onFailed(
                        CronetAdaptiveNetworkBidirectionalStream.this, info, error);
                mLogger.logCronetAdaptiveTrafficTerminated(mLoggingState);
            }
        }

        @Override
        public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
            checkValidStream(stream);
            if (stream == mPrimaryStream) {
                mLoggingState.setMainRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_CANCELLED);
            } else {
                mLoggingState.setFallbackRequestState(
                        CronetLogger.CronetAdaptiveTrafficRequestState
                                .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_CANCELLED);
            }

            // We are in one of two scenarios:
            // 1. The active stream was cancelled. In this case we forward to the backend
            //    callback, without having to worry about the other stream.
            // 2. The non-active stream was cancelled. In this case we need to worry about
            //    whether the other stream ever became active. This is handled via
            //    mOnlyOneStreamRemains, which is only set by non-active streams.
            if (mActiveStream.get() == stream || mOnlyOneStreamRemains.getAndSet(true)) {
                mBackendCallback.onCanceled(CronetAdaptiveNetworkBidirectionalStream.this, info);
                mLogger.logCronetAdaptiveTrafficTerminated(mLoggingState);
            }
        }
    }

    private final class RaceUntilOnResponseHeadersReceivedCallback
            extends RaceUntilOnStreamReadyCallback {
        /** When an Idempotent stream becomes ready, we replay all the buffered writes on it. */
        @Override
        public void onStreamReady(BidirectionalStream stream) {
            checkValidStream(stream);
            synchronized (mLock) {
                mReadyStreams.add(stream);
                for (PendingWrite pendingWrite : mPendingWrites) {
                    ByteBuffer replayedBuffer = pendingWrite.mBuffer.duplicate();
                    mReplayedBuffers.add(replayedBuffer);
                    stream.write(replayedBuffer, pendingWrite.mEndOfStream);
                }
            }
            // TODO(b/474048542): Should we really call flush here? The outer stream might not have
            // been flushed yet.
            stream.flush();
            if (mCallbackOnStreamReadyCalled.compareAndSet(false, true)) {
                mBackendCallback.onStreamReady(CronetAdaptiveNetworkBidirectionalStream.this);
            }
        }

        @Override
        public void onResponseHeadersReceived(BidirectionalStream stream, UrlResponseInfo info) {
            checkValidStream(stream);
            if (mActiveStream.compareAndSet(null, stream)) {
                synchronized (mLock) {
                    mReadyStreams.add(stream);
                }
                setActiveStreamWinner(stream);
            }
            super.onResponseHeadersReceived(stream, info);
        }

        @Override
        public void onWriteCompleted(
                BidirectionalStream stream,
                UrlResponseInfo info,
                ByteBuffer buffer,
                boolean endOfStream) {
            checkValidStream(stream);
            synchronized (mLock) {
                if (mReplayedBuffers.remove(buffer)) {
                    return;
                }
            }
            // TODO(b/474048542): If the main stream hangs and does not report onWriteCompleted, the
            // `mBackendCallback.onWriteCompleted` may never be called even though the fallback
            // stream calls onWriteCompleted.
            if (mActiveStream.get() == stream || mActiveStream.get() == null) {
                mBackendCallback.onWriteCompleted(
                        CronetAdaptiveNetworkBidirectionalStream.this, info, buffer, endOfStream);
            }
        }
    }

    public CronetAdaptiveNetworkBidirectionalStream(
            BidirectionalStream.Callback backendCallback,
            ScheduledExecutorService scheduledExecutor,
            CronetAdaptiveRequestContext adaptiveRequestContext,
            String url,
            CronetLogger logger,
            boolean isFastIdempotentRequest) {
        mExecutor = scheduledExecutor;
        mBackendCallback = new VersionSafeCallbacks.BidirectionalStreamCallback(backendCallback);
        mAdaptiveRequestContext = adaptiveRequestContext;
        mLogger = logger;
        mUrl = url;
        mIsFastIdempotentRequest = isFastIdempotentRequest;
        mFallbackStream = null;
        mOnlyOneStreamRemains = new AtomicBoolean(false);
        mRedirectingCallback =
                isFastIdempotentRequest
                        ? new RaceUntilOnResponseHeadersReceivedCallback()
                        : new RaceUntilOnStreamReadyCallback();
    }

    void setPrimaryStream(CronetBidirectionalStream primaryStream) {
        mPrimaryStream = Objects.requireNonNull(primaryStream);
    }

    void setFallbackStream(CronetBidirectionalStream fallbackStream) {
        mFallbackStream = Objects.requireNonNull(fallbackStream);
    }

    @Override
    public void start() {
        Objects.requireNonNull(mPrimaryStream, "Primary stream is required before starting.")
                .start();
        mLoggingState.setMainRequestState(
                CronetLogger.CronetAdaptiveTrafficRequestState
                        .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_STARTED);
        Objects.requireNonNull(mFallbackStream, "Fallback stream is required before starting.");
        mFailoverFuture.set(
                mExecutor.schedule(
                        this::maybeScheduleFastFailover,
                        mAdaptiveRequestContext.getReadyFailoverMs(),
                        MILLISECONDS));
    }

    private void maybeScheduleFastFailover() {
        if (mActiveStream.get() == null) {
            mFallbackStream.start();
            mLoggingState.setFallbackRequestState(
                    CronetLogger.CronetAdaptiveTrafficRequestState
                            .CRONET_ADAPTIVE_TRAFFIC_REQUEST_STATE_STARTED);
        }
        // Clear the mFailoverFuture.
        if (mFailoverFuture.getAndSet(null) == null) {
            throw new AssertionError(
                    "maybeScheduleFastFailover called without active failover future.");
        }
    }

    @Override
    public void read(ByteBuffer buffer) {
        Objects.requireNonNull(mActiveStream.get()).read(buffer);
    }

    @Override
    public void write(ByteBuffer buffer, boolean endOfStream) {
        if (!mIsFastIdempotentRequest) {
            // TODO(b/474048542): Writing without mActiveStream should technically be allowed.
            Objects.requireNonNull(mActiveStream.get()).write(buffer, endOfStream);
            return;
        }
        // Continue with mIsFastIdempotentRequest logic.
        synchronized (mLock) {
            // TODO(b/474048542): Do not duplicate ByteBuffers here
            // to ensure onWriteCompleted gets called properly even on multiple writes.
            mPendingWrites.add(new PendingWrite(buffer, endOfStream));
            for (BidirectionalStream stream : mReadyStreams) {
                ByteBuffer replayedBuffer = buffer.duplicate();
                mReplayedBuffers.add(replayedBuffer);
                stream.write(replayedBuffer, endOfStream);
            }
        }
    }

    @Override
    public void flush() {
        if (!mIsFastIdempotentRequest) {
            Objects.requireNonNull(mActiveStream.get()).flush();
            return;
        }
        // Continue with mIsFastIdempotentRequest logic.
        synchronized (mLock) {
            for (BidirectionalStream stream : mReadyStreams) {
                stream.flush();
            }
        }
    }

    @Override
    public void cancel() {
        cancelFailover();
        Objects.requireNonNull(mPrimaryStream).cancel();
    }

    @Override
    public boolean isDone() {
        if (mActiveStream.get() != null) {
            return mActiveStream.get().isDone();
        }
        return mPrimaryStream.isDone() && mFallbackStream.isDone();
    }

    private void cancelFailover() {
        ScheduledFuture<?> future = mFailoverFuture.get();
        if (future == null) {
            // Either this#start has not been called yet OR maybeScheduleFastFailover has reached
            // the point of clearing the mFailoverFuture (so mFallbackStream#start has been called).
            // In both scenarios it's fine to just forward the call to the underlying fallback
            // stream (this is what would happen in a regular BidirectionalStream).
            Objects.requireNonNull(mFallbackStream, "Cancel attempted without fallback stream set")
                    .cancel();
            return;
        }
        // this#start has been called and the mFailoverFuture set.
        // We have to either:
        // 1. If mFallbackStream#start has not been called yet, guarantee we will never call it by
        //    canceling the future.
        if (!future.cancel(/* mayInterruptIfRunning= */ false)) {
            // 2. OR if we cannot cancel the future (because it's running), ensure that
            // mFallbackStream#cancel is called after mFallbackStream#start by posting the cancel on
            // the executor that runs
            // the mFailoverFuture.
            mExecutor.execute(
                    () -> {
                        mFallbackStream.cancel();
                    });
        }
    }

    BidirectionalStream.Callback getCallback() {
        return mRedirectingCallback;
    }

    private void setActiveStreamWinner(BidirectionalStream stream) {
        if (stream == mFallbackStream) {
            mLoggingState.setWinner(
                    CronetLogger.CronetAdaptiveTrafficWinner
                            .CRONET_ADAPTIVE_TRAFFIC_WINNER_FALLBACK);
            // The primary stream was not ready in time, let's cancel it.
            mPrimaryStream.cancel();
            long networkHandle = mFallbackStream.getTargetNetworkHandle();
            if (networkHandle != CronetEngineBase.DEFAULT_NETWORK_HANDLE) {
                mAdaptiveRequestContext.reportFallbackUsed(mUrl, networkHandle);
            }
        } else {
            mLoggingState.setWinner(
                    CronetLogger.CronetAdaptiveTrafficWinner.CRONET_ADAPTIVE_TRAFFIC_WINNER_MAIN);
            // Cancel the failover stream.
            cancelFailover();
        }
    }

    private void checkValidStream(BidirectionalStream stream) {
        if (stream != mPrimaryStream && stream != mFallbackStream) {
            // We can only handle the primary and fallback stream. Getting any other stream would
            // mean there's a bug.
            throw new AssertionError("Callback stream neither primary nor fallback.");
        }
    }
}
