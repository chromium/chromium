// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static java.lang.Math.max;

import android.os.Build;
import android.os.Process;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.net.BidirectionalStream;
import org.chromium.net.CallbackException;
import org.chromium.net.ConnectionCloseSource;
import org.chromium.net.CronetException;
import org.chromium.net.ExperimentalBidirectionalStream;
import org.chromium.net.NetworkException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.RequestPriority;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;

import javax.annotation.concurrent.GuardedBy;
import javax.security.auth.callback.Callback;

/**
 * {@link BidirectionalStream} implementation using Chromium network stack. All @CalledByNative
 * methods are called on the native network thread and post tasks with callback calls onto Executor.
 * Upon returning from callback, the native stream is called on Executor thread and posts native
 * tasks to the native network thread.
 */
@JNINamespace("cronet")
@VisibleForTesting
public class CronetBidirectionalStream extends ExperimentalBidirectionalStream {
    /**
     * States of BidirectionalStream are tracked in mReadState and mWriteState.
     * The write state is separated out as it changes independently of the read state.
     * There is one initial state: State.NOT_STARTED. There is one normal final state:
     * State.SUCCESS, reached after State.READING_DONE and State.WRITING_DONE. There are two
     * exceptional final states: State.CANCELED and State.ERROR, which can be reached from
     * any other non-final state.
     */
    @IntDef({
        State.NOT_STARTED,
        State.STARTED,
        State.WAITING_FOR_READ,
        State.READING,
        State.READING_DONE,
        State.CANCELED,
        State.ERROR,
        State.SUCCESS,
        State.WAITING_FOR_FLUSH,
        State.WRITING,
        State.WRITING_DONE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface State {
        /* Initial state, stream not started. */
        int NOT_STARTED = 0;
        /*
         * Stream started, request headers are being sent if mDelayRequestHeadersUntilNextFlush
         * is not set to true.
         */
        int STARTED = 1;
        /* Waiting for {@code read()} to be called. */
        int WAITING_FOR_READ = 2;
        /* Reading from the remote, {@code onReadCompleted()} callback will be called when done. */
        int READING = 3;
        /* There is no more data to read and stream is half-closed by the remote side. */
        int READING_DONE = 4;
        /* Stream is canceled. */
        int CANCELED = 5;
        /* Error has occurred, stream is closed. */
        int ERROR = 6;
        /* Reading and writing are done, and the stream is closed successfully. */
        int SUCCESS = 7;
        /* Waiting for {@code CronetBidirectionalStreamJni.get().sendRequestHeaders()} or {@code
        CronetBidirectionalStreamJni.get().writevData()} to be called. */
        int WAITING_FOR_FLUSH = 8;
        /* Writing to the remote, {@code onWritevCompleted()} callback will be called when done. */
        int WRITING = 9;
        /* There is no more data to write and stream is half-closed by the local side. */
        int WRITING_DONE = 10;
    }

    private final CronetUrlRequestContext mRequestContext;
    private final Executor mExecutor;
    private final VersionSafeCallbacks.BidirectionalStreamCallback mCallback;
    private final String mInitialUrl;
    private final int mInitialPriority;
    private final String mInitialMethod;
    private final String[] mRequestHeaders;
    private final boolean mDelayRequestHeadersUntilFirstFlush;
    private final Collection<Object> mRequestAnnotations;
    private final boolean mTrafficStatsTagSet;
    private final int mTrafficStatsTag;
    private final boolean mTrafficStatsUidSet;
    private final int mTrafficStatsUid;
    private final long mNetworkHandle;
    private final CronetLogger mLogger;
    private RefCountDelegate mInflightDoneCallbackCount;
    private CronetException mException;
    private int mNonfinalUserCallbackExceptionCount;
    private int mReadCount;
    private int mFlushCount;
    private boolean mFinalUserCallbackThrew;

    /*
     * Synchronizes access to mNativeStream, mReadState and mWriteState.
     */
    private final Object mNativeStreamLock = new Object();

    @GuardedBy("mNativeStreamLock")
    // Pending write data.
    private LinkedList<ByteBuffer> mPendingData;

    @GuardedBy("mNativeStreamLock")
    // Flush data queue that should be pushed to the native stack when the previous
    // CronetBidirectionalStreamJni.get().writevData completes.
    private LinkedList<ByteBuffer> mFlushData;

    @GuardedBy("mNativeStreamLock")
    // Whether an end-of-stream flag is passed in through write().
    private boolean mEndOfStreamWritten;

    @GuardedBy("mNativeStreamLock")
    // Whether request headers have been sent.
    private boolean mRequestHeadersSent;

    // Metrics information. Obtained when request succeeds, fails or is canceled.
    private CronetMetrics mMetrics;
    private boolean mQuicConnectionMigrationAttempted;
    private boolean mQuicConnectionMigrationSuccessful;

    /* Native BidirectionalStream object, owned by CronetBidirectionalStream. */
    @GuardedBy("mNativeStreamLock")
    private long mNativeStream;

    /**
     * Read state is tracking reading flow.
     *                         / <--- READING <--- \
     *                         |                   |
     *                         \                   /
     * NOT_STARTED -> STARTED --> WAITING_FOR_READ -> READING_DONE -> SUCCESS
     */
    @GuardedBy("mNativeStreamLock")
    private @State int mReadState = State.NOT_STARTED;

    /**
     * Write state is tracking writing flow.
     *                         / <---  WRITING  <--- \
     *                         |                     |
     *                         \                     /
     * NOT_STARTED -> STARTED --> WAITING_FOR_FLUSH -> WRITING_DONE -> SUCCESS
     */
    @GuardedBy("mNativeStreamLock")
    private @State int mWriteState = State.NOT_STARTED;

    // Only modified on the network thread.
    private UrlResponseInfoImpl mResponseInfo;

    /*
     * OnReadCompleted callback is repeatedly invoked when each read is completed, so it
     * is cached as a member variable.
     */
    // Only modified on the network thread.
    private OnReadCompletedRunnable mOnReadCompletedTask;

    private Runnable mOnDestroyedCallbackForTesting;

    private final class OnReadCompletedRunnable implements Runnable {
        // Buffer passed back from current invocation of onReadCompleted.
        ByteBuffer mByteBuffer;
        // End of stream flag from current invocation of onReadCompleted.
        boolean mEndOfStream;

        @Override
        public void run() {
            try {
                // Null out mByteBuffer, to pass buffer ownership to callback or release if done.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                boolean maybeOnSucceeded = false;
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    if (mEndOfStream) {
                        mReadState = State.READING_DONE;
                        maybeOnSucceeded = (mWriteState == State.WRITING_DONE);
                    } else {
                        mReadState = State.WAITING_FOR_READ;
                    }
                }
                mCallback.onReadCompleted(
                        CronetBidirectionalStream.this, mResponseInfo, buffer, mEndOfStream);
                if (maybeOnSucceeded) {
                    maybeOnSucceededOnExecutor();
                }
            } catch (Exception e) {
                onNonfinalCallbackException(e);
            }
        }
    }

    private final class OnWriteCompletedRunnable implements Runnable {
        // Buffer passed back from current invocation of onWriteCompleted.
        private ByteBuffer mByteBuffer;
        // End of stream flag from current call to write.
        private final boolean mEndOfStream;

        OnWriteCompletedRunnable(ByteBuffer buffer, boolean endOfStream) {
            mByteBuffer = buffer;
            mEndOfStream = endOfStream;
        }

        @Override
        public void run() {
            try {
                // Null out mByteBuffer, to pass buffer ownership to callback or release if done.
                ByteBuffer buffer = mByteBuffer;
                mByteBuffer = null;
                boolean maybeOnSucceeded = false;
                synchronized (mNativeStreamLock) {
                    if (isDoneLocked()) {
                        return;
                    }
                    if (mEndOfStream) {
                        mWriteState = State.WRITING_DONE;
                        maybeOnSucceeded = (mReadState == State.READING_DONE);
                    }
                }
                mCallback.onWriteCompleted(
                        CronetBidirectionalStream.this, mResponseInfo, buffer, mEndOfStream);
                if (maybeOnSucceeded) {
                    maybeOnSucceededOnExecutor();
                }
            } catch (Exception e) {
                onNonfinalCallbackException(e);
            }
        }
    }

    CronetBidirectionalStream(
            CronetUrlRequestContext requestContext,
            String url,
            @CronetEngineBase.StreamPriority int priority,
            Callback callback,
            Executor executor,
            String httpMethod,
            List<Map.Entry<String, String>> requestHeaders,
            boolean delayRequestHeadersUntilNextFlush,
            Collection<Object> requestAnnotations,
            boolean trafficStatsTagSet,
            int trafficStatsTag,
            boolean trafficStatsUidSet,
            int trafficStatsUid,
            long networkHandle) {
        mRequestContext = requestContext;
        mInitialUrl = url;
        mInitialPriority = convertStreamPriority(priority);
        mCallback = new VersionSafeCallbacks.BidirectionalStreamCallback(callback);
        mExecutor = executor;
        mInitialMethod = httpMethod;
        mRequestHeaders = stringsFromHeaderList(requestHeaders);
        mDelayRequestHeadersUntilFirstFlush = delayRequestHeadersUntilNextFlush;
        mPendingData = new LinkedList<>();
        mFlushData = new LinkedList<>();
        mRequestAnnotations = requestAnnotations;
        mTrafficStatsTagSet = trafficStatsTagSet;
        mTrafficStatsTag = trafficStatsTag;
        mTrafficStatsUidSet = trafficStatsUidSet;
        mTrafficStatsUid = trafficStatsUid;
        mNetworkHandle = networkHandle;
        mLogger = requestContext.getCronetLogger();
    }

    @Override
    public void start() {
        synchronized (mNativeStreamLock) {
            if (mReadState != State.NOT_STARTED) {
                throw new IllegalStateException("Stream is already started.");
            }
            try {
                mNativeStream =
                        CronetBidirectionalStreamJni.get()
                                .createBidirectionalStream(
                                        CronetBidirectionalStream.this,
                                        mRequestContext.getUrlRequestContextAdapter(),
                                        !mDelayRequestHeadersUntilFirstFlush,
                                        mTrafficStatsTagSet,
                                        mTrafficStatsTag,
                                        mTrafficStatsUidSet,
                                        mTrafficStatsUid,
                                        mNetworkHandle);
                // Non-zero startResult means an argument error.
                int startResult =
                        CronetBidirectionalStreamJni.get()
                                .start(
                                        mNativeStream,
                                        CronetBidirectionalStream.this,
                                        mInitialUrl,
                                        mInitialPriority,
                                        mInitialMethod,
                                        mRequestHeaders,
                                        !doesMethodAllowWriteData(mInitialMethod));
                if (startResult == -1) {
                    throw new IllegalArgumentException("Invalid http method " + mInitialMethod);
                }
                if (startResult > 0) {
                    int headerPos = startResult - 1;
                    throw new IllegalArgumentException(
                            "Invalid header with headername: " + mRequestHeaders[headerPos]);
                }

                mRequestContext.onRequestStarted();
                mInflightDoneCallbackCount = new RefCountDelegate(this::onRequestFinished);
                // We need an initial count of 2: one decrement for the final callback
                // (e.g. onSucceeded), and another for onMetricsCollected().
                mInflightDoneCallbackCount.increment();
                mReadState = mWriteState = State.STARTED;
            } catch (RuntimeException e) {
                // If there's an exception, clean up and then throw the
                // exception to the caller.
                destroyNativeStreamLocked(false);
                throw e;
            }
        }
    }

    @Override
    public void read(ByteBuffer buffer) {
        synchronized (mNativeStreamLock) {
            Preconditions.checkHasRemaining(buffer);
            Preconditions.checkDirect(buffer);
            if (mReadState != State.WAITING_FOR_READ) {
                throw new IllegalStateException("Unexpected read attempt.");
            }
            if (isDoneLocked()) {
                return;
            }
            if (mOnReadCompletedTask == null) {
                mOnReadCompletedTask = new OnReadCompletedRunnable();
            }
            mReadState = State.READING;
            if (!CronetBidirectionalStreamJni.get()
                    .readData(
                            mNativeStream,
                            CronetBidirectionalStream.this,
                            buffer,
                            buffer.position(),
                            buffer.limit())) {
                // Still waiting on read. This is just to have consistent
                // behavior with the other error cases.
                mReadState = State.WAITING_FOR_READ;
                throw new IllegalArgumentException("Unable to call native read");
            }
            mReadCount++;
        }
    }

    @Override
    public void write(ByteBuffer buffer, boolean endOfStream) {
        synchronized (mNativeStreamLock) {
            Preconditions.checkDirect(buffer);
            if (!buffer.hasRemaining() && !endOfStream) {
                throw new IllegalArgumentException("Empty buffer before end of stream.");
            }
            if (mEndOfStreamWritten) {
                throw new IllegalArgumentException("Write after writing end of stream.");
            }
            if (isDoneLocked()) {
                return;
            }
            mPendingData.add(buffer);
            if (endOfStream) {
                mEndOfStreamWritten = true;
            }
        }
    }

    @Override
    public void flush() {
        synchronized (mNativeStreamLock) {
            if (isDoneLocked()
                    || (mWriteState != State.WAITING_FOR_FLUSH && mWriteState != State.WRITING)) {
                return;
            }
            if (mPendingData.isEmpty() && mFlushData.isEmpty()) {
                // If there is no pending write when flush() is called, see if
                // request headers need to be flushed.
                if (!mRequestHeadersSent) {
                    mRequestHeadersSent = true;
                    CronetBidirectionalStreamJni.get()
                            .sendRequestHeaders(mNativeStream, CronetBidirectionalStream.this);
                    if (!doesMethodAllowWriteData(mInitialMethod)) {
                        mWriteState = State.WRITING_DONE;
                    }
                }
                return;
            }

            assert !mPendingData.isEmpty() || !mFlushData.isEmpty();

            // Move buffers from mPendingData to the flushing queue.
            if (!mPendingData.isEmpty()) {
                mFlushData.addAll(mPendingData);
                mPendingData.clear();
            }

            if (mWriteState == State.WRITING) {
                // If there is a write already pending, wait until onWritevCompleted is
                // called before pushing data to the native stack.
                return;
            }
            sendFlushDataLocked();
            mFlushCount++;
        }
    }

    // Helper method to send buffers in mFlushData. Caller needs to acquire
    // mNativeStreamLock and make sure mWriteState is WAITING_FOR_FLUSH and
    // mFlushData queue isn't empty.
    @SuppressWarnings("GuardedByChecker")
    private void sendFlushDataLocked() {
        assert mWriteState == State.WAITING_FOR_FLUSH;
        int size = mFlushData.size();
        ByteBuffer[] buffers = new ByteBuffer[size];
        int[] positions = new int[size];
        int[] limits = new int[size];
        for (int i = 0; i < size; i++) {
            ByteBuffer buffer = mFlushData.poll();
            buffers[i] = buffer;
            positions[i] = buffer.position();
            limits[i] = buffer.limit();
        }
        assert mFlushData.isEmpty();
        assert buffers.length >= 1;
        mWriteState = State.WRITING;
        mRequestHeadersSent = true;
        if (!CronetBidirectionalStreamJni.get()
                .writevData(
                        mNativeStream,
                        CronetBidirectionalStream.this,
                        buffers,
                        positions,
                        limits,
                        mEndOfStreamWritten && mPendingData.isEmpty())) {
            // Still waiting on flush. This is just to have consistent
            // behavior with the other error cases.
            mWriteState = State.WAITING_FOR_FLUSH;
            throw new IllegalArgumentException("Unable to call native writev.");
        }
    }

    /** Returns a read-only copy of {@code mPendingData} for testing. */
    public List<ByteBuffer> getPendingDataForTesting() {
        synchronized (mNativeStreamLock) {
            List<ByteBuffer> pendingData = new LinkedList<ByteBuffer>();
            for (ByteBuffer buffer : mPendingData) {
                pendingData.add(buffer.asReadOnlyBuffer());
            }
            return pendingData;
        }
    }

    /** Returns a read-only copy of {@code mFlushData} for testing. */
    public List<ByteBuffer> getFlushDataForTesting() {
        synchronized (mNativeStreamLock) {
            List<ByteBuffer> flushData = new LinkedList<ByteBuffer>();
            for (ByteBuffer buffer : mFlushData) {
                flushData.add(buffer.asReadOnlyBuffer());
            }
            return flushData;
        }
    }

    @Override
    public void cancel() {
        synchronized (mNativeStreamLock) {
            if (isDoneLocked() || mReadState == State.NOT_STARTED) {
                return;
            }
            mReadState = mWriteState = State.CANCELED;
            destroyNativeStreamLocked(true);
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mNativeStreamLock) {
            return isDoneLocked();
        }
    }

    @GuardedBy("mNativeStreamLock")
    private boolean isDoneLocked() {
        return mReadState != State.NOT_STARTED && mNativeStream == 0;
    }

    /*
     * Runs an onSucceeded callback if both Read and Write sides are closed.
     */
    private void maybeOnSucceededOnExecutor() {
        synchronized (mNativeStreamLock) {
            if (isDoneLocked()) {
                return;
            }
            if (!(mWriteState == State.WRITING_DONE && mReadState == State.READING_DONE)) {
                return;
            }
            mReadState = mWriteState = State.SUCCESS;
            // Destroy native stream first, so UrlRequestContext could be shut
            // down from the listener.
            destroyNativeStreamLocked(false);
        }
        try {
            mCallback.onSucceeded(CronetBidirectionalStream.this, mResponseInfo);
        } catch (Exception e) {
            onFinalCallbackException("onSucceeded", e);
        }
        mInflightDoneCallbackCount.decrement();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onStreamReady(final boolean requestHeadersSent) {
        postTaskToExecutor(
                new Runnable() {
                    @Override
                    public void run() {
                        synchronized (mNativeStreamLock) {
                            if (isDoneLocked()) {
                                return;
                            }
                            mRequestHeadersSent = requestHeadersSent;
                            mReadState = State.WAITING_FOR_READ;
                            if (!doesMethodAllowWriteData(mInitialMethod) && mRequestHeadersSent) {
                                mWriteState = State.WRITING_DONE;
                            } else {
                                mWriteState = State.WAITING_FOR_FLUSH;
                            }
                        }

                        try {
                            mCallback.onStreamReady(CronetBidirectionalStream.this);
                        } catch (Exception e) {
                            onNonfinalCallbackException(e);
                        }
                    }
                });
    }

    /**
     * Called when the final set of headers, after all redirects,
     * is received. Can only be called once for each stream.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseHeadersReceived(
            int httpStatusCode,
            String negotiatedProtocol,
            String[] headers,
            long receivedByteCount) {
        try {
            mResponseInfo =
                    prepareResponseInfoOnNetworkThread(
                            httpStatusCode, negotiatedProtocol, headers, receivedByteCount);
        } catch (Exception e) {
            failWithException(new CronetExceptionImpl("Cannot prepare ResponseInfo", null));
            return;
        }
        postTaskToExecutor(
                new Runnable() {
                    @Override
                    public void run() {
                        synchronized (mNativeStreamLock) {
                            if (isDoneLocked()) {
                                return;
                            }
                            mReadState = State.WAITING_FOR_READ;
                        }

                        try {
                            mCallback.onResponseHeadersReceived(
                                    CronetBidirectionalStream.this, mResponseInfo);
                        } catch (Exception e) {
                            onNonfinalCallbackException(e);
                        }
                    }
                });
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onReadCompleted(
            final ByteBuffer byteBuffer,
            int bytesRead,
            int initialPosition,
            int initialLimit,
            long receivedByteCount) {
        mResponseInfo.setReceivedByteCount(receivedByteCount);
        if (byteBuffer.position() != initialPosition || byteBuffer.limit() != initialLimit) {
            failWithException(
                    new CronetExceptionImpl("ByteBuffer modified externally during read", null));
            return;
        }
        if (bytesRead < 0 || initialPosition + bytesRead > initialLimit) {
            failWithException(new CronetExceptionImpl("Invalid number of bytes read", null));
            return;
        }
        byteBuffer.position(initialPosition + bytesRead);
        assert mOnReadCompletedTask.mByteBuffer == null;
        mOnReadCompletedTask.mByteBuffer = byteBuffer;
        mOnReadCompletedTask.mEndOfStream = (bytesRead == 0);
        postTaskToExecutor(mOnReadCompletedTask);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onWritevCompleted(
            final ByteBuffer[] byteBuffers,
            int[] initialPositions,
            int[] initialLimits,
            boolean endOfStream) {
        assert byteBuffers.length == initialPositions.length;
        assert byteBuffers.length == initialLimits.length;
        synchronized (mNativeStreamLock) {
            if (isDoneLocked()) return;
            mWriteState = State.WAITING_FOR_FLUSH;
            // Flush if there is anything in the flush queue mFlushData.
            if (!mFlushData.isEmpty()) {
                sendFlushDataLocked();
            }
        }
        for (int i = 0; i < byteBuffers.length; i++) {
            ByteBuffer buffer = byteBuffers[i];
            if (buffer.position() != initialPositions[i] || buffer.limit() != initialLimits[i]) {
                failWithException(
                        new CronetExceptionImpl(
                                "ByteBuffer modified externally during write", null));
                return;
            }
            // Current implementation always writes the complete buffer.
            buffer.position(buffer.limit());
            postTaskToExecutor(
                    new OnWriteCompletedRunnable(
                            buffer,
                            // Only set endOfStream flag if this buffer is the last in byteBuffers.
                            endOfStream && i == byteBuffers.length - 1));
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onResponseTrailersReceived(String[] trailers) {
        final UrlResponseInfo.HeaderBlock trailersBlock =
                new UrlResponseInfoImpl.HeaderBlockImpl(headersListFromStrings(trailers));
        postTaskToExecutor(
                new Runnable() {
                    @Override
                    public void run() {
                        synchronized (mNativeStreamLock) {
                            if (isDoneLocked()) {
                                return;
                            }
                        }
                        try {
                            mCallback.onResponseTrailersReceived(
                                    CronetBidirectionalStream.this, mResponseInfo, trailersBlock);
                        } catch (Exception e) {
                            onNonfinalCallbackException(e);
                        }
                    }
                });
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onError(
            int errorCode,
            int nativeError,
            int nativeQuicError,
            @ConnectionCloseSource int source,
            String errorString,
            long receivedByteCount) {
        if (mResponseInfo != null) {
            mResponseInfo.setReceivedByteCount(receivedByteCount);
        }
        if (errorCode == NetworkException.ERROR_QUIC_PROTOCOL_FAILED || nativeQuicError != 0) {
            failWithException(
                    new QuicExceptionImpl(
                            "Exception in BidirectionalStream: " + errorString,
                            errorCode,
                            nativeError,
                            nativeQuicError,
                            source));
        } else {
            failWithException(
                    new BidirectionalStreamNetworkException(
                            "Exception in BidirectionalStream: " + errorString,
                            errorCode,
                            nativeError));
        }
    }

    /** Called when request is canceled, no callbacks will be called afterwards. */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onCanceled() {
        postTaskToExecutor(
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            mCallback.onCanceled(CronetBidirectionalStream.this, mResponseInfo);
                        } catch (Exception e) {
                            onFinalCallbackException("onCanceled", e);
                        }
                        mInflightDoneCallbackCount.decrement();
                    }
                });
    }

    /**
     * Called by the native code, from the network thread, immediately before the native adapter
     * destroys itself. Not called if the native adapter was never started.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void onMetricsCollected(
            long requestStartMs,
            long dnsStartMs,
            long dnsEndMs,
            long connectStartMs,
            long connectEndMs,
            long sslStartMs,
            long sslEndMs,
            long sendingStartMs,
            long sendingEndMs,
            long pushStartMs,
            long pushEndMs,
            long responseStartMs,
            long requestEndMs,
            boolean socketReused,
            long sentByteCount,
            long receivedByteCount,
            boolean quicConnectionMigrationAttempted,
            boolean quicConnectionMigrationSuccessful) {
        try {
            if (mMetrics != null) {
                throw new IllegalStateException("Metrics collection should only happen once.");
            }
            mMetrics =
                    new CronetMetrics(
                            requestStartMs,
                            dnsStartMs,
                            dnsEndMs,
                            connectStartMs,
                            connectEndMs,
                            sslStartMs,
                            sslEndMs,
                            sendingStartMs,
                            sendingEndMs,
                            pushStartMs,
                            pushEndMs,
                            responseStartMs,
                            requestEndMs,
                            socketReused,
                            sentByteCount,
                            receivedByteCount);
            mQuicConnectionMigrationAttempted = quicConnectionMigrationAttempted;
            mQuicConnectionMigrationSuccessful = quicConnectionMigrationSuccessful;
            final RequestFinishedInfo requestFinishedInfo =
                    new RequestFinishedInfoImpl(
                            mInitialUrl,
                            mRequestAnnotations,
                            mMetrics,
                            getFinishedReason(),
                            mResponseInfo,
                            mException);
            mRequestContext.reportRequestFinished(
                    requestFinishedInfo, mInflightDoneCallbackCount, null);
        } finally {
            mInflightDoneCallbackCount.decrement();
        }
    }

    /**
     * Explains why the request finished. Can only be called after the request has reached a
     * terminal state.
     */
    // No need for synchronization as the read/write states are not supposed to change at this point
    @SuppressWarnings("GuardedBy")
    private @RequestFinishedInfoImpl.FinishedReason int getFinishedReason() {
        if (mReadState != mWriteState) {
            throw new IllegalStateException(
                    "Cronet bidirectional stream read state is "
                            + mReadState
                            + " which is different from write state "
                            + mWriteState
                            + "!");
        }
        switch (mReadState) {
            case State.SUCCESS:
                return RequestFinishedInfo.SUCCEEDED;
            case State.CANCELED:
                return RequestFinishedInfo.CANCELED;
            case State.ERROR:
                return RequestFinishedInfo.FAILED;
            default:
                throw new IllegalStateException(
                        "Cronet bidirectional stream read state is "
                                + mReadState
                                + " which is not a valid finished state!");
        }
    }

    private void onRequestFinished() {
        // Before we can log, we need to wait for both onMetricsCollected() *and* the final user
        // callback to return, because we get data from both (e.g. the latter provides final user
        // callback exception info). These two code paths run concurrently, so we can't just log
        // from one or the other without racing. Instead we do this from the "request finished" code
        // path which is guaranteed to run after both are done.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mLogger.logCronetTrafficInfo(
                    mRequestContext.getLogId(),
                    buildCronetTrafficInfo(
                            getFinishedReason(),
                            mQuicConnectionMigrationAttempted,
                            mQuicConnectionMigrationSuccessful));
        }
        mRequestContext.onRequestFinished();
    }

    private CronetTrafficInfo buildCronetTrafficInfo(
            @RequestFinishedInfoImpl.FinishedReason int finishedReason,
            boolean quicConnectionMigrationAttempted,
            boolean quicConnectionMigrationSuccessful) {
        assert mMetrics != null;
        assert mRequestHeaders != null;

        // Most of the CronetTrafficInfo fields have similar names/semantics. To avoid bugs due to
        // typos everything is final, this means that things have to initialized through an if/else.
        final Map<String, List<String>> responseHeaders;
        final String negotiatedProtocol;
        final int httpStatusCode;
        final boolean wasCached;
        if (mResponseInfo != null) {
            responseHeaders = mResponseInfo.getAllHeaders();
            negotiatedProtocol = mResponseInfo.getNegotiatedProtocol();
            httpStatusCode = mResponseInfo.getHttpStatusCode();
            wasCached = mResponseInfo.wasCached();
        } else {
            responseHeaders = Collections.emptyMap();
            negotiatedProtocol = "";
            httpStatusCode = 0;
            wasCached = false;
        }

        // TODO(stefanoduo): A better approach might be keeping track of the total length of an
        // upload and use that value as the request body size instead.
        final long requestTotalSizeInBytes = mMetrics.getSentByteCount();
        final long requestHeaderSizeInBytes;
        final long requestBodySizeInBytes;
        // Cached responses might still need to be revalidated over the network before being served
        // (from UrlResponseInfo#wasCached documentation).
        if (wasCached && requestTotalSizeInBytes == 0) {
            // Served from cache without the need to revalidate.
            requestHeaderSizeInBytes = 0;
            requestBodySizeInBytes = 0;
        } else {
            // Served from cache with the need to revalidate or served from the network directly.
            requestHeaderSizeInBytes =
                    CronetRequestCommon.estimateHeadersSizeInBytes(mRequestHeaders);
            requestBodySizeInBytes = max(0, requestTotalSizeInBytes - requestHeaderSizeInBytes);
        }

        final long responseTotalSizeInBytes = mMetrics.getReceivedByteCount();
        final long responseBodySizeInBytes;
        final long responseHeaderSizeInBytes;
        // Cached responses might still need to be revalidated over the network before being served
        // (from UrlResponseInfo#wasCached documentation).
        if (wasCached && responseTotalSizeInBytes == 0) {
            // Served from cache without the need to revalidate.
            responseBodySizeInBytes = 0;
            responseHeaderSizeInBytes = 0;
        } else {
            // Served from cache with the need to revalidate or served from the network directly.
            responseHeaderSizeInBytes =
                    CronetRequestCommon.estimateHeadersSizeInBytes(responseHeaders);
            responseBodySizeInBytes = max(0, responseTotalSizeInBytes - responseHeaderSizeInBytes);
        }

        final Duration headersLatency;
        if (mMetrics.getRequestStart() != null && mMetrics.getResponseStart() != null) {
            headersLatency =
                    Duration.ofMillis(
                            mMetrics.getResponseStart().getTime()
                                    - mMetrics.getRequestStart().getTime());
        } else {
            headersLatency = Duration.ofSeconds(0);
        }

        final Duration totalLatency;
        if (mMetrics.getRequestStart() != null && mMetrics.getRequestEnd() != null) {
            totalLatency =
                    Duration.ofMillis(
                            mMetrics.getRequestEnd().getTime()
                                    - mMetrics.getRequestStart().getTime());
        } else {
            totalLatency = Duration.ofSeconds(0);
        }

        int networkInternalErrorCode = 0;
        int quicNetworkErrorCode = 0;
        @ConnectionCloseSource int source = ConnectionCloseSource.UNKNOWN;
        CronetTrafficInfo.RequestFailureReason failureReason =
                CronetTrafficInfo.RequestFailureReason.UNKNOWN;

        // Going through the API layer will lead to NoSuchMethodError exceptions
        // because there is no guarantee that the API will have the method.
        // It's possible to use an old API of Cronet with a new implementation.
        // In order to work around this, only impl classes are mentioned
        // to ensure that the methods will always be found.
        // See b/361725824 for more information.
        if (mException instanceof NetworkExceptionImpl networkException) {
            networkInternalErrorCode = networkException.getCronetInternalErrorCode();
            failureReason = CronetTrafficInfo.RequestFailureReason.NETWORK;
        } else if (mException instanceof QuicExceptionImpl quicException) {
            networkInternalErrorCode = quicException.getCronetInternalErrorCode();
            quicNetworkErrorCode = quicException.getQuicDetailedErrorCode();
            source = quicException.getConnectionCloseSource();
            failureReason = CronetTrafficInfo.RequestFailureReason.NETWORK;
        } else if (mException != null) {
            failureReason = CronetTrafficInfo.RequestFailureReason.OTHER;
        }

        return new CronetTrafficInfo(
                requestHeaderSizeInBytes,
                requestBodySizeInBytes,
                responseHeaderSizeInBytes,
                responseBodySizeInBytes,
                httpStatusCode,
                headersLatency,
                totalLatency,
                negotiatedProtocol,
                quicConnectionMigrationAttempted,
                quicConnectionMigrationSuccessful,
                CronetRequestCommon.finishedReasonToCronetTrafficInfoRequestTerminalState(
                        finishedReason),
                mNonfinalUserCallbackExceptionCount,
                mReadCount,
                mFlushCount,
                /* isBidiStream= */ true,
                mFinalUserCallbackThrew,
                Process.myUid(),
                networkInternalErrorCode,
                quicNetworkErrorCode,
                source,
                failureReason,
                mMetrics.getSocketReused());
    }

    public void setOnDestroyedCallbackForTesting(Runnable onDestroyedCallbackForTesting) {
        mOnDestroyedCallbackForTesting = onDestroyedCallbackForTesting;
    }

    private static boolean doesMethodAllowWriteData(String methodName) {
        return !methodName.equals("GET") && !methodName.equals("HEAD");
    }

    private static ArrayList<Map.Entry<String, String>> headersListFromStrings(String[] headers) {
        ArrayList<Map.Entry<String, String>> headersList = new ArrayList<>(headers.length / 2);
        for (int i = 0; i < headers.length; i += 2) {
            headersList.add(new AbstractMap.SimpleImmutableEntry<>(headers[i], headers[i + 1]));
        }
        return headersList;
    }

    private static String[] stringsFromHeaderList(List<Map.Entry<String, String>> headersList) {
        String[] headersArray = new String[headersList.size() * 2];
        int i = 0;
        for (Map.Entry<String, String> requestHeader : headersList) {
            headersArray[i++] = requestHeader.getKey();
            headersArray[i++] = requestHeader.getValue();
        }
        return headersArray;
    }

    private static int convertStreamPriority(@CronetEngineBase.StreamPriority int priority) {
        switch (priority) {
            case Builder.STREAM_PRIORITY_IDLE:
                return RequestPriority.IDLE;
            case Builder.STREAM_PRIORITY_LOWEST:
                return RequestPriority.LOWEST;
            case Builder.STREAM_PRIORITY_LOW:
                return RequestPriority.LOW;
            case Builder.STREAM_PRIORITY_MEDIUM:
                return RequestPriority.MEDIUM;
            case Builder.STREAM_PRIORITY_HIGHEST:
                return RequestPriority.HIGHEST;
            default:
                throw new IllegalArgumentException("Invalid stream priority.");
        }
    }

    /**
     * Posts task to application Executor. Used for callbacks
     * and other tasks that should not be executed on network thread.
     */
    private void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (RejectedExecutionException failException) {
            Log.e(
                    CronetUrlRequestContext.LOG_TAG,
                    "Exception posting task to executor",
                    failException);
            // If posting a task throws an exception, then there is no choice
            // but to destroy the stream without invoking the callback.
            synchronized (mNativeStreamLock) {
                mReadState = mWriteState = State.ERROR;
                destroyNativeStreamLocked(false);
            }
        }
    }

    private UrlResponseInfoImpl prepareResponseInfoOnNetworkThread(
            int httpStatusCode,
            String negotiatedProtocol,
            String[] headers,
            long receivedByteCount) {
        UrlResponseInfoImpl responseInfo =
                new UrlResponseInfoImpl(
                        Arrays.asList(mInitialUrl),
                        httpStatusCode,
                        "",
                        headersListFromStrings(headers),
                        false,
                        negotiatedProtocol,
                        null,
                        receivedByteCount);
        return responseInfo;
    }

    @GuardedBy("mNativeStreamLock")
    private void destroyNativeStreamLocked(boolean sendOnCanceled) {
        Log.i(CronetUrlRequestContext.LOG_TAG, "destroyNativeStreamLocked " + this.toString());
        if (mNativeStream == 0) {
            return;
        }
        CronetBidirectionalStreamJni.get()
                .destroy(mNativeStream, CronetBidirectionalStream.this, sendOnCanceled);
        var readStarted = mReadState != State.NOT_STARTED;
        var writeStarted = mWriteState != State.NOT_STARTED;
        assert readStarted == writeStarted;
        if (readStarted) {
            mRequestContext.onRequestDestroyed();
        }
        mNativeStream = 0;
        if (mOnDestroyedCallbackForTesting != null) {
            mOnDestroyedCallbackForTesting.run();
        }
    }

    /** Fails the stream with an exception. Only called on the Executor. */
    private void failWithExceptionOnExecutor(CronetException e) {
        mException = e;
        // Do not call into mCallback if request is complete.
        synchronized (mNativeStreamLock) {
            if (isDoneLocked()) {
                return;
            }
            mReadState = mWriteState = State.ERROR;
            destroyNativeStreamLocked(false);
        }
        try {
            mCallback.onFailed(this, mResponseInfo, e);
        } catch (Exception failException) {
            onFinalCallbackException("onFailed", failException);
        }
        mInflightDoneCallbackCount.decrement();
    }

    /**
     * If a non-final callback method throws an exception, stream gets canceled and exception is
     * reported via onFailed callback. Only called on the Executor.
     */
    private void onNonfinalCallbackException(Exception e) {
        mNonfinalUserCallbackExceptionCount++;
        CallbackException streamError =
                new CallbackExceptionImpl("CalledByNative method has thrown an exception", e);
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in CalledByNative method", e);
        failWithExceptionOnExecutor(streamError);
    }

    /** Fails the stream with an exception. Can be called on any thread. */
    private void failWithException(final CronetException exception) {
        postTaskToExecutor(
                new Runnable() {
                    @Override
                    public void run() {
                        failWithExceptionOnExecutor(exception);
                    }
                });
    }

    private void onFinalCallbackException(String method, Exception e) {
        mFinalUserCallbackThrew = true;
        Log.e(CronetUrlRequestContext.LOG_TAG, "Exception in " + method + " method", e);
    }

    @NativeMethods
    interface Natives {
        // Native methods are implemented in cronet_bidirectional_stream_adapter.cc.
        long createBidirectionalStream(
                CronetBidirectionalStream caller,
                long urlRequestContextAdapter,
                boolean sendRequestHeadersAutomatically,
                boolean trafficStatsTagSet,
                int trafficStatsTag,
                boolean trafficStatsUidSet,
                int trafficStatsUid,
                long networkHandle);

        @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
        int start(
                long nativePtr,
                CronetBidirectionalStream caller,
                String url,
                int priority,
                String method,
                String[] headers,
                boolean endOfStream);

        @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
        void sendRequestHeaders(long nativePtr, CronetBidirectionalStream caller);

        @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
        boolean readData(
                long nativePtr,
                CronetBidirectionalStream caller,
                ByteBuffer byteBuffer,
                int position,
                int limit);

        @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
        boolean writevData(
                long nativePtr,
                CronetBidirectionalStream caller,
                ByteBuffer[] buffers,
                int[] positions,
                int[] limits,
                boolean endOfStream);

        @NativeClassQualifiedName("CronetBidirectionalStreamAdapter")
        void destroy(long nativePtr, CronetBidirectionalStream caller, boolean sendOnCanceled);
    }
}
