// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import androidx.annotation.Nullable;

import org.chromium.net.BidirectionalStream;
import org.chromium.net.CronetException;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicReference;
import java.util.Objects;

/**
 * A {@link BidirectionalStream} implementation that allows fallback to an alternative stream if the
 * primary stream is not ready within a certain timeout.
 */
final class CronetAdaptiveNetworkBidirectionalStream extends ExperimentalBidirectionalStream {
    /**
     * The time we wait until we give up the connection attempt and start the backup stream. This
     * value is 3x the initial retransmit timeout for TCP, we assume that a connection with
     * reasonable performance will be open within this timeframe.
     * TODO(crbug.com/474048542): This should be a constructor parameter hooked up to flag.
     */
    private static final long READY_FAILOVER_MS = 3000;

    private ExperimentalBidirectionalStream mPrimaryStream;
    private @Nullable ExperimentalBidirectionalStream mFallbackStream;

    private final ScheduledExecutorService mExecutor;
    private final AtomicReference<BidirectionalStream> mActiveStream = new AtomicReference<>(null);
    /** The developer facing callback. */
    private final BidirectionalStream.Callback mBackendCallback;

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
                        } else if (mFallbackStream != null) {
                            // We have a fallback stream, but it's not needed.
                            // Explicitly cancel it to avoid it being used later.
                            mFallbackStream.cancel();
                        }
                        mBackendCallback.onStreamReady(CronetAdaptiveNetworkBidirectionalStream.this);
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
                    mBackendCallback.onSucceeded(CronetAdaptiveNetworkBidirectionalStream.this, info);
                }

                @Override
                public void onFailed(
                        BidirectionalStream stream, UrlResponseInfo info, CronetException error) {
                    checkValidStream(stream);
                    if (mActiveStream.get() != stream) {
                        return;
                    }
                    mBackendCallback.onFailed(CronetAdaptiveNetworkBidirectionalStream.this, info, error);
                }

                @Override
                public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
                    checkValidStream(stream);
                    if (mActiveStream.get() != stream) {
                        return;
                    }
                    mBackendCallback.onCanceled(CronetAdaptiveNetworkBidirectionalStream.this, info);
                }
            };

    public CronetAdaptiveNetworkBidirectionalStream(
            BidirectionalStream.Callback backendCallback, ScheduledExecutorService scheduledExecutor) {
        mExecutor = scheduledExecutor;
        mBackendCallback = backendCallback;
        mFallbackStream = null;
    }

    void setPrimaryStream(CronetBidirectionalStream primaryStream) {
        mPrimaryStream = Objects.requireNonNull(primaryStream);
    }

    void setFallbackStream(CronetBidirectionalStream fallbackStream) {
        mFallbackStream = Objects.requireNonNull(fallbackStream);
    }

    @Override
    public void start() {
        Objects.requireNonNull(mPrimaryStream).start();
        // If a fallback stream was created, schedule a potential future switch to it.
        if (mFallbackStream != null) {
            mExecutor.schedule(() -> maybeScheduleFastFailover(), READY_FAILOVER_MS, MILLISECONDS);
        }
    }

    private void maybeScheduleFastFailover() {
        if (mActiveStream.get() == null) {
            mFallbackStream.start();
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
        Objects.requireNonNull(mPrimaryStream).cancel();
        if (mFallbackStream != null) {
            mFallbackStream.cancel();
        }
    }

    @Override
    public boolean isDone() {
        if (mActiveStream.get() == null) {
            return false;
        }
        return mActiveStream.get().isDone();
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
