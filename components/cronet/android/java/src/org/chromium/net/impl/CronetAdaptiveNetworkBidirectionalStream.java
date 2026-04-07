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
import java.util.Objects;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A {@link BidirectionalStream} implementation that allows fallback to an alternative stream if the
 * primary stream is not ready within a certain timeout.
 */
final class CronetAdaptiveNetworkBidirectionalStream extends ExperimentalBidirectionalStream {
    @VisibleForTesting CronetBidirectionalStream mPrimaryStream;
    private final AtomicBoolean mOnlyOneStreamRemains;
    @VisibleForTesting CronetBidirectionalStream mFallbackStream;

    private final ScheduledExecutorService mExecutor;
    private final AtomicReference<BidirectionalStream> mActiveStream = new AtomicReference<>(null);

    private final AtomicReference<ScheduledFuture<?>> mFailoverFuture = new AtomicReference<>();

    /** The developer facing callback. */
    private final BidirectionalStream.Callback mBackendCallback;

    private final CronetAdaptiveRequestContext mAdaptiveRequestContext;
    private final String mUrl;

    /** The callback passed to both streams - picking the winner and calling mBackendCallback. */
    private final BidirectionalStream.Callback mRedirectingCallback =
            new BidirectionalStream.Callback() {
                /**
                 * This callback happens on both the primary and fallback stream. But the stream
                 * being ready first "wins" and will be used for future operations.
                 */
                @Override
                public void onStreamReady(BidirectionalStream stream) {
                    checkValidStream(stream);
                    if (mActiveStream.compareAndSet(null, stream)) {
                        if (stream == mFallbackStream) {
                            // The primary stream was not ready in time, let's cancel it.
                            mPrimaryStream.cancel();
                            long networkHandle = mFallbackStream.getTargetNetworkHandle();
                            if (networkHandle != CronetEngineBase.DEFAULT_NETWORK_HANDLE) {
                                mAdaptiveRequestContext.reportFallbackUsed(mUrl, networkHandle);
                            }
                        } else {
                            // Cancel the failover stream.
                            cancelFailover();
                        }
                        mBackendCallback.onStreamReady(
                                CronetAdaptiveNetworkBidirectionalStream.this);
                    }
                }

                @Override
                public void onResponseHeadersReceived(
                        BidirectionalStream stream, UrlResponseInfo info) {
                    checkValidStream(stream);
                    mBackendCallback.onResponseHeadersReceived(
                            CronetAdaptiveNetworkBidirectionalStream.this, info);
                }

                @Override
                public void onReadCompleted(
                        BidirectionalStream stream,
                        UrlResponseInfo info,
                        ByteBuffer buffer,
                        boolean endOfStream) {
                    checkValidStream(stream);
                    mBackendCallback.onReadCompleted(
                            CronetAdaptiveNetworkBidirectionalStream.this,
                            info,
                            buffer,
                            endOfStream);
                }

                @Override
                public void onWriteCompleted(
                        BidirectionalStream stream,
                        UrlResponseInfo info,
                        ByteBuffer buffer,
                        boolean endOfStream) {
                    checkValidStream(stream);
                    mBackendCallback.onWriteCompleted(
                            CronetAdaptiveNetworkBidirectionalStream.this,
                            info,
                            buffer,
                            endOfStream);
                }

                @Override
                public void onResponseTrailersReceived(
                        BidirectionalStream stream,
                        UrlResponseInfo info,
                        UrlResponseInfo.HeaderBlock trailers) {
                    checkValidStream(stream);
                    mBackendCallback.onResponseTrailersReceived(
                            CronetAdaptiveNetworkBidirectionalStream.this, info, trailers);
                }

                @Override
                public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
                    checkValidStream(stream);
                    mBackendCallback.onSucceeded(
                            CronetAdaptiveNetworkBidirectionalStream.this, info);
                }

                @Override
                public void onFailed(
                        BidirectionalStream stream, UrlResponseInfo info, CronetException error) {
                    checkValidStream(stream);
                    // The active stream has failed.
                    if (mActiveStream.get() == stream) {
                        mBackendCallback.onFailed(
                                CronetAdaptiveNetworkBidirectionalStream.this, info, error);
                        return;
                    }
                    // Either the primary stream just failed or we are the second stream to fail.
                    // Time to give up and signal failure.
                    if (mOnlyOneStreamRemains.getAndSet(true)) {
                        mBackendCallback.onFailed(
                                CronetAdaptiveNetworkBidirectionalStream.this, info, error);
                        return;
                    }
                }

                @Override
                public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
                    checkValidStream(stream);
                    // The active stream was cancelled.
                    if (mActiveStream.get() == stream) {
                        mBackendCallback.onCanceled(
                                CronetAdaptiveNetworkBidirectionalStream.this, info);
                        return;
                    }
                    // Either the primary stream just cancelled or we are the second stream to
                    // cancel. Signal the cancel.
                    if (mOnlyOneStreamRemains.getAndSet(true)) {
                        mBackendCallback.onCanceled(
                                CronetAdaptiveNetworkBidirectionalStream.this, info);
                        return;
                    }
                }
            };

    public CronetAdaptiveNetworkBidirectionalStream(
            BidirectionalStream.Callback backendCallback,
            ScheduledExecutorService scheduledExecutor,
            CronetAdaptiveRequestContext adaptiveRequestContext,
            String url) {
        mExecutor = scheduledExecutor;
        mBackendCallback = backendCallback;
        mAdaptiveRequestContext = adaptiveRequestContext;
        mUrl = url;
        mFallbackStream = null;
        mOnlyOneStreamRemains = new AtomicBoolean(false);
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
        Objects.requireNonNull(mActiveStream.get()).write(buffer, endOfStream);
    }

    @Override
    public void flush() {
        Objects.requireNonNull(mActiveStream.get()).flush();
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
            // the point
            // of clearing the mFailoverFuture (so mFallbackStream#start has been called).
            // In both scenarios it's fine to just forward the call
            // to the underlying fallback stream (this is what would happen in a
            // regular BidirectionalStream).
            Objects.requireNonNull(mFallbackStream, "Cancel attempted without fallback stream set")
                    .cancel();
            return;
        }
        // this#start has been called and the mFailoverFuture set.
        // We have to either:
        // 1. If mFallbackStream#start has not been called yet, guarantee we will never call
        // it by canceling the future.
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

    private void checkValidStream(BidirectionalStream stream) {
        if (stream != mPrimaryStream && stream != mFallbackStream) {
            // We can only handle the primary and fallback stream. Getting any other stream would
            // mean there's a bug.
            throw new AssertionError("Callback stream neither primary nor fallback.");
        }
    }
}
