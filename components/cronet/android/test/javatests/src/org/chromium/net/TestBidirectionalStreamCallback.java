// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import android.os.ConditionVariable;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;

/**
 * Callback that tracks information from different callbacks and and has a
 * method to block thread until the stream completes on another thread.
 * Allows to cancel, block stream or throw an exception from an arbitrary step.
 */
public class TestBidirectionalStreamCallback extends BidirectionalStream.Callback {
    public UrlResponseInfo mResponseInfo;
    public CronetException mError;

    public ResponseStep mResponseStep = ResponseStep.NOTHING;

    public boolean mOnErrorCalled;
    public boolean mOnCanceledCalled;

    public int mHttpResponseDataLength;
    public String mResponseAsString = "";

    public UrlResponseInfo.HeaderBlock mTrailers;

    private static final int READ_BUFFER_SIZE = 32 * 1024;

    // When false, the consumer is responsible for all calls into the stream
    // that advance it.
    private boolean mAutoAdvance = true;

    // The executor thread will block on this after reaching a terminal method.
    // Terminal methods are (onSucceeded, onFailed or onCancelled)
    private ConditionVariable mBlockOnTerminalState = new ConditionVariable(true);

    // Conditionally fail on certain steps.
    private FailureType mFailureType = FailureType.NONE;
    private ResponseStep mFailureStep = ResponseStep.NOTHING;

    // Signals when the stream is done either successfully or not.
    private final ConditionVariable mDone = new ConditionVariable();

    // Signaled on each step when mAutoAdvance is false.
    private final ConditionVariable mReadStepBlock = new ConditionVariable();
    private final ConditionVariable mWriteStepBlock = new ConditionVariable();

    // Executor Service for Cronet callbacks.
    private final ExecutorService mExecutorService =
            Executors.newSingleThreadExecutor(new ExecutorThreadFactory());
    private Thread mExecutorThread;

    // position() of ByteBuffer prior to read() call.
    private int mBufferPositionBeforeRead;

    // Data to write.
    private final ArrayList<WriteBuffer> mWriteBuffers = new ArrayList<WriteBuffer>();

    // Buffers that we yet to receive the corresponding onWriteCompleted callback.
    private final ArrayList<WriteBuffer> mWriteBuffersToBeAcked = new ArrayList<WriteBuffer>();

    // Whether to use a direct executor.
    private final boolean mUseDirectExecutor;
    private final DirectExecutor mDirectExecutor;

    private class ExecutorThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(Runnable r) {
            mExecutorThread = new Thread(r);
            return mExecutorThread;
        }
    }

    private static class WriteBuffer {
        final ByteBuffer mBuffer;
        final boolean mFlush;
        public WriteBuffer(ByteBuffer buffer, boolean flush) {
            mBuffer = buffer;
            mFlush = flush;
        }
    }

    private static class DirectExecutor implements Executor {
        @Override
        public void execute(Runnable task) {
            task.run();
        }
    }

    public enum ResponseStep {
        NOTHING,
        ON_STREAM_READY,
        ON_RESPONSE_STARTED,
        ON_READ_COMPLETED,
        ON_WRITE_COMPLETED,
        ON_TRAILERS,
        ON_CANCELED,
        ON_FAILED,
        ON_SUCCEEDED,
    }

    public enum FailureType {
        NONE,
        CANCEL_SYNC,
        CANCEL_ASYNC,
        // Same as above, but continues to advance the stream after posting
        // the cancellation task.
        CANCEL_ASYNC_WITHOUT_PAUSE,
        THROW_SYNC
    }

    public TestBidirectionalStreamCallback() {
        mUseDirectExecutor = false;
        mDirectExecutor = null;
    }

    public TestBidirectionalStreamCallback(boolean useDirectExecutor) {
        mUseDirectExecutor = useDirectExecutor;
        mDirectExecutor = new DirectExecutor();
    }

    /**
     * This blocks the callback executor thread once it has reached a final state callback.
     * In order to continue execution, this method must be called again and providing {@code false}
     * to continue execution.
     * @param blockOnTerminalState the state to set for the executor thread
     */
    public void setBlockOnTerminalState(boolean blockOnTerminalState) {
        if (blockOnTerminalState) {
            mBlockOnTerminalState.close();
        } else {
            mBlockOnTerminalState.open();
        }
    }

    public void setAutoAdvance(boolean autoAdvance) {
        mAutoAdvance = autoAdvance;
    }

    public void setFailure(FailureType failureType, ResponseStep failureStep) {
        mFailureStep = failureStep;
        mFailureType = failureType;
    }

    public void blockForDone() {
        mDone.block();
    }

    public void waitForNextReadStep() {
        mReadStepBlock.block();
        mReadStepBlock.close();
    }

    public void waitForNextWriteStep() {
        mWriteStepBlock.block();
        mWriteStepBlock.close();
    }

    public Executor getExecutor() {
        if (mUseDirectExecutor) {
            return mDirectExecutor;
        }
        return mExecutorService;
    }

    public void shutdownExecutor() {
        if (mUseDirectExecutor) {
            throw new UnsupportedOperationException("DirectExecutor doesn't support shutdown");
        }
        mExecutorService.shutdown();
    }

    public void addWriteData(byte[] data) {
        addWriteData(data, true);
    }

    public void addWriteData(byte[] data, boolean flush) {
        ByteBuffer writeBuffer = ByteBuffer.allocateDirect(data.length);
        writeBuffer.put(data);
        writeBuffer.flip();
        mWriteBuffers.add(new WriteBuffer(writeBuffer, flush));
        mWriteBuffersToBeAcked.add(new WriteBuffer(writeBuffer, flush));
    }

    @Override
    public void onStreamReady(BidirectionalStream stream) {
        checkOnValidThread();
        assertFalse(stream.isDone());
        assertThat(mResponseStep).isEqualTo(ResponseStep.NOTHING);
        assertNull(mError);
        mResponseStep = ResponseStep.ON_STREAM_READY;
        if (maybeThrowCancelOrPause(stream, mWriteStepBlock)) {
            return;
        }
        startNextWrite(stream);
    }

    @Override
    public void onResponseHeadersReceived(BidirectionalStream stream, UrlResponseInfo info) {
        checkOnValidThread();
        assertFalse(stream.isDone());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                || mResponseStep == ResponseStep.ON_STREAM_READY
                || mResponseStep == ResponseStep.ON_WRITE_COMPLETED);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_RESPONSE_STARTED;
        mResponseInfo = info;
        if (maybeThrowCancelOrPause(stream, mReadStepBlock)) {
            return;
        }
        startNextRead(stream);
    }

    @Override
    public void onReadCompleted(BidirectionalStream stream, UrlResponseInfo info,
            ByteBuffer byteBuffer, boolean endOfStream) {
        checkOnValidThread();
        assertFalse(stream.isDone());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                || mResponseStep == ResponseStep.ON_READ_COMPLETED
                || mResponseStep == ResponseStep.ON_WRITE_COMPLETED
                || mResponseStep == ResponseStep.ON_TRAILERS);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_READ_COMPLETED;
        mResponseInfo = info;

        final int bytesRead = byteBuffer.position() - mBufferPositionBeforeRead;
        mHttpResponseDataLength += bytesRead;
        final byte[] lastDataReceivedAsBytes = new byte[bytesRead];
        // Rewind byteBuffer.position() to pre-read() position.
        byteBuffer.position(mBufferPositionBeforeRead);
        // This restores byteBuffer.position() to its value on entrance to
        // this function.
        byteBuffer.get(lastDataReceivedAsBytes);

        mResponseAsString += new String(lastDataReceivedAsBytes);

        if (maybeThrowCancelOrPause(stream, mReadStepBlock)) {
            return;
        }
        // Do not read if EOF has been reached.
        if (!endOfStream) {
            startNextRead(stream);
        }
    }

    @Override
    public void onWriteCompleted(BidirectionalStream stream, UrlResponseInfo info,
            ByteBuffer buffer, boolean endOfStream) {
        checkOnValidThread();
        assertFalse(stream.isDone());
        assertNull(mError);
        mResponseStep = ResponseStep.ON_WRITE_COMPLETED;
        mResponseInfo = info;
        if (!mWriteBuffersToBeAcked.isEmpty()) {
            assertThat(mWriteBuffersToBeAcked.get(0).mBuffer).isEqualTo(buffer);
            mWriteBuffersToBeAcked.remove(0);
        }
        if (maybeThrowCancelOrPause(stream, mWriteStepBlock)) {
            return;
        }
        startNextWrite(stream);
    }

    @Override
    public void onResponseTrailersReceived(BidirectionalStream stream, UrlResponseInfo info,
            UrlResponseInfo.HeaderBlock trailers) {
        checkOnValidThread();
        assertFalse(stream.isDone());
        assertNull(mError);
        mResponseStep = ResponseStep.ON_TRAILERS;
        mResponseInfo = info;
        mTrailers = trailers;
        if (maybeThrowCancelOrPause(stream, mReadStepBlock)) {
            return;
        }
    }

    @Override
    public void onSucceeded(BidirectionalStream stream, UrlResponseInfo info) {
        checkOnValidThread();
        assertTrue(stream.isDone());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                || mResponseStep == ResponseStep.ON_READ_COMPLETED
                || mResponseStep == ResponseStep.ON_WRITE_COMPLETED
                || mResponseStep == ResponseStep.ON_TRAILERS);
        assertFalse(mOnErrorCalled);
        assertFalse(mOnCanceledCalled);
        assertNull(mError);
        assertThat(mWriteBuffers).isEmpty();
        assertThat(mWriteBuffersToBeAcked).isEmpty();

        mResponseStep = ResponseStep.ON_SUCCEEDED;
        mResponseInfo = info;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(stream, mReadStepBlock);
    }

    @Override
    public void onFailed(BidirectionalStream stream, UrlResponseInfo info, CronetException error) {
        checkOnValidThread();
        assertTrue(stream.isDone());
        // Shouldn't happen after success.
        assertTrue(mResponseStep != ResponseStep.ON_SUCCEEDED);
        // Should happen at most once for a single stream.
        assertFalse(mOnErrorCalled);
        assertFalse(mOnCanceledCalled);
        assertNull(mError);
        mResponseStep = ResponseStep.ON_FAILED;
        mResponseInfo = info;

        mOnErrorCalled = true;
        mError = error;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(stream, mReadStepBlock);
    }

    @Override
    public void onCanceled(BidirectionalStream stream, UrlResponseInfo info) {
        checkOnValidThread();
        assertTrue(stream.isDone());
        // Should happen at most once for a single stream.
        assertFalse(mOnCanceledCalled);
        assertFalse(mOnErrorCalled);
        assertNull(mError);
        mResponseStep = ResponseStep.ON_CANCELED;
        mResponseInfo = info;

        mOnCanceledCalled = true;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(stream, mReadStepBlock);
    }

    public void startNextRead(BidirectionalStream stream) {
        startNextRead(stream, ByteBuffer.allocateDirect(READ_BUFFER_SIZE));
    }

    public void startNextRead(BidirectionalStream stream, ByteBuffer buffer) {
        mBufferPositionBeforeRead = buffer.position();
        stream.read(buffer);
    }

    public void startNextWrite(BidirectionalStream stream) {
        if (!mWriteBuffers.isEmpty()) {
            Iterator<WriteBuffer> iterator = mWriteBuffers.iterator();
            while (iterator.hasNext()) {
                WriteBuffer b = iterator.next();
                stream.write(b.mBuffer, !iterator.hasNext());
                iterator.remove();
                if (b.mFlush) {
                    stream.flush();
                    break;
                }
            }
        }
    }

    public boolean isDone() {
        // It's not mentioned by the Android docs, but block(0) seems to block
        // indefinitely, so have to block for one millisecond to get state
        // without blocking.
        return mDone.block(1);
    }

    /**
     * Returns the number of pending Writes.
     */
    public int numPendingWrites() {
        return mWriteBuffers.size();
    }

    protected void openDone() {
        mDone.open();
    }

    /**
     * Returns {@code false} if the callback should continue to advance the
     * stream.
     */
    private boolean maybeThrowCancelOrPause(
            final BidirectionalStream stream, ConditionVariable stepBlock) {
        if (mResponseStep != mFailureStep || mFailureType == FailureType.NONE) {
            if (!mAutoAdvance) {
                stepBlock.open();
                return true;
            }
            return false;
        }

        if (mFailureType == FailureType.THROW_SYNC) {
            throw new IllegalStateException("Callback Exception.");
        }
        Runnable task = new Runnable() {
            @Override
            public void run() {
                stream.cancel();
            }
        };
        if (mFailureType == FailureType.CANCEL_ASYNC
                || mFailureType == FailureType.CANCEL_ASYNC_WITHOUT_PAUSE) {
            getExecutor().execute(task);
        } else {
            task.run();
        }
        return mFailureType != FailureType.CANCEL_ASYNC_WITHOUT_PAUSE;
    }

    /**
     * Checks whether callback methods are invoked on the correct thread.
     */
    private void checkOnValidThread() {
        if (!mUseDirectExecutor) {
            assertThat(Thread.currentThread()).isEqualTo(mExecutorThread);
        }
    }
}
