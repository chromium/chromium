// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

import android.os.ConditionVariable;
import android.os.StrictMode;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

/**
 * Callback that tracks information from different callbacks and and has a
 * method to block thread until the request completes on another thread.
 * Allows to cancel, block request or throw an exception from an arbitrary step.
 */
public class TestUrlRequestCallback extends UrlRequest.Callback {
    public ArrayList<UrlResponseInfo> mRedirectResponseInfoList = new ArrayList<UrlResponseInfo>();
    public ArrayList<String> mRedirectUrlList = new ArrayList<String>();
    private UrlResponseInfo mResponseInfo;
    public CronetException mError;

    public ResponseStep mResponseStep = ResponseStep.NOTHING;

    public int mRedirectCount;
    public boolean mOnErrorCalled;
    public boolean mOnCanceledCalled;

    public int mHttpResponseDataLength;
    public String mResponseAsString = "";

    public int mReadBufferSize = 32 * 1024;

    // When false, the consumer is responsible for all calls into the request
    // that advance it.
    private boolean mAutoAdvance = true;
    // Whether an exception is thrown by maybeThrowCancelOrPause().
    private boolean mCallbackExceptionThrown;

    // Whether to permit calls on the network thread.
    private boolean mAllowDirectExecutor;

    // The executor thread will block on this after reaching a terminal method.
    // Terminal methods are (onSucceeded, onFailed or onCancelled)
    private ConditionVariable mBlockOnTerminalState = new ConditionVariable(true);

    // Conditionally fail on certain steps.
    private FailureType mFailureType = FailureType.NONE;
    private ResponseStep mFailureStep = ResponseStep.NOTHING;

    // Signals when request is done either successfully or not.
    private final ConditionVariable mDone = new ConditionVariable();

    // Signaled on each step when mAutoAdvance is false.
    private final ConditionVariable mStepBlock = new ConditionVariable();

    // Executor Service for Cronet callbacks.
    private final ExecutorService mExecutorService;
    private Thread mExecutorThread;

    // position() of ByteBuffer prior to read() call.
    private int mBufferPositionBeforeRead;

    private static class ExecutorThreadFactory implements ThreadFactory {
        @Override
        public Thread newThread(final Runnable r) {
            return new Thread(
                    new Runnable() {
                        @Override
                        public void run() {
                            StrictMode.ThreadPolicy threadPolicy = StrictMode.getThreadPolicy();
                            try {
                                StrictMode.setThreadPolicy(
                                        new StrictMode.ThreadPolicy.Builder()
                                                .detectNetwork()
                                                .penaltyLog()
                                                .penaltyDeath()
                                                .build());
                                r.run();
                            } finally {
                                StrictMode.setThreadPolicy(threadPolicy);
                            }
                        }
                    });
        }
    }

    public enum ResponseStep {
        NOTHING,
        ON_RECEIVED_REDIRECT,
        ON_RESPONSE_STARTED,
        ON_READ_COMPLETED,
        ON_SUCCEEDED,
        ON_FAILED,
        ON_CANCELED,
    }

    public enum FailureType {
        NONE,
        CANCEL_SYNC,
        CANCEL_ASYNC,
        // Same as above, but continues to advance the request after posting
        // the cancellation task.
        CANCEL_ASYNC_WITHOUT_PAUSE,
        THROW_SYNC
    }

    /** Set {@code mExecutorThread}. */
    private void fillInExecutorThread() {
        mExecutorService.execute(
                new Runnable() {
                    @Override
                    public void run() {
                        mExecutorThread = Thread.currentThread();
                    }
                });
    }

    /** Create a {@link TestUrlRequestCallback} with a new single-threaded executor. */
    public TestUrlRequestCallback() {
        this(Executors.newSingleThreadExecutor(new ExecutorThreadFactory()));
    }

    /**
     * Create a {@link TestUrlRequestCallback} using a custom single-threaded executor.
     * NOTE(pauljensen): {@code executorService} should be a new single-threaded executor.
     */
    public TestUrlRequestCallback(ExecutorService executorService) {
        mExecutorService = executorService;
        fillInExecutorThread();
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

    public void setAllowDirectExecutor(boolean allowed) {
        mAllowDirectExecutor = allowed;
    }

    public void setFailure(FailureType failureType, ResponseStep failureStep) {
        mFailureStep = failureStep;
        mFailureType = failureType;
    }

    public void blockForDone() {
        mDone.block();
    }

    public void blockForDone(long timeoutMs) {
        assertWithMessage("Request didn't terminate in time").that(mDone.block(timeoutMs)).isTrue();
    }

    public void waitForNextStep() {
        mStepBlock.block();
        mStepBlock.close();
    }

    public ExecutorService getExecutor() {
        return mExecutorService;
    }

    public void shutdownExecutor() {
        mExecutorService.shutdown();
    }

    /**
     * Shuts down the ExecutorService and waits until it executes all posted
     * tasks.
     */
    public void shutdownExecutorAndWait() {
        mExecutorService.shutdown();
        try {
            // Termination shouldn't take long. Use 1 min which should be more than enough.
            mExecutorService.awaitTermination(1, TimeUnit.MINUTES);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        assertThat(mExecutorService.isTerminated()).isTrue();
    }

    @Override
    public void onRedirectReceived(
            UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
        checkExecutorThread();
        assertThat(request.isDone()).isFalse();
        assertThat(mResponseStep).isAnyOf(ResponseStep.NOTHING, ResponseStep.ON_RECEIVED_REDIRECT);
        assertThat(mError).isNull();

        mResponseStep = ResponseStep.ON_RECEIVED_REDIRECT;
        mRedirectUrlList.add(newLocationUrl);
        mRedirectResponseInfoList.add(info);
        ++mRedirectCount;
        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        request.followRedirect();
    }

    @Override
    public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
        checkExecutorThread();
        assertThat(request.isDone()).isFalse();
        assertThat(mResponseStep).isAnyOf(ResponseStep.NOTHING, ResponseStep.ON_RECEIVED_REDIRECT);
        assertThat(mError).isNull();

        mResponseStep = ResponseStep.ON_RESPONSE_STARTED;
        mResponseInfo = info;
        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        startNextRead(request);
    }

    @Override
    public void onReadCompleted(UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
        checkExecutorThread();
        assertThat(request.isDone()).isFalse();
        assertThat(mResponseStep)
                .isAnyOf(ResponseStep.ON_RESPONSE_STARTED, ResponseStep.ON_READ_COMPLETED);
        assertThat(mError).isNull();

        mResponseStep = ResponseStep.ON_READ_COMPLETED;

        final byte[] lastDataReceivedAsBytes;
        final int bytesRead = byteBuffer.position() - mBufferPositionBeforeRead;
        mHttpResponseDataLength += bytesRead;
        lastDataReceivedAsBytes = new byte[bytesRead];
        // Rewind |byteBuffer.position()| to pre-read() position.
        byteBuffer.position(mBufferPositionBeforeRead);
        // This restores |byteBuffer.position()| to its value on entrance to
        // this function.
        byteBuffer.get(lastDataReceivedAsBytes);
        mResponseAsString += new String(lastDataReceivedAsBytes);

        if (maybeThrowCancelOrPause(request)) {
            return;
        }
        startNextRead(request);
    }

    @Override
    public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
        checkExecutorThread();
        assertThat(request.isDone()).isTrue();
        assertThat(mResponseStep)
                .isAnyOf(ResponseStep.ON_RESPONSE_STARTED, ResponseStep.ON_READ_COMPLETED);
        assertThat(mOnErrorCalled).isFalse();
        assertThat(mOnCanceledCalled).isFalse();
        assertThat(mError).isNull();

        mResponseStep = ResponseStep.ON_SUCCEEDED;
        mResponseInfo = info;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(request);
    }

    @Override
    public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
        // If the failure is because of prohibited direct execution, the test shouldn't fail
        // since the request already did.
        if (error.getCause() instanceof InlineExecutionProhibitedException) {
            mAllowDirectExecutor = true;
        }
        checkExecutorThread();
        assertThat(request.isDone()).isTrue();
        // Shouldn't happen after success.
        assertThat(mResponseStep).isNotEqualTo(ResponseStep.ON_SUCCEEDED);
        // Should happen at most once for a single request.
        assertThat(mError).isNull();
        assertThat(mOnErrorCalled).isFalse();
        assertThat(mOnCanceledCalled).isFalse();
        if (mCallbackExceptionThrown) {
            assertThat(error).isInstanceOf(CallbackException.class);
            assertThat(error)
                    .hasMessageThat()
                    .contains("Exception received from UrlRequest.Callback");
            assertThat(error).hasCauseThat().isInstanceOf(IllegalStateException.class);
            assertThat(error).hasCauseThat().hasMessageThat().contains("Listener Exception.");
        }

        mResponseStep = ResponseStep.ON_FAILED;
        mOnErrorCalled = true;
        mError = error;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(request);
    }

    @Override
    public void onCanceled(UrlRequest request, UrlResponseInfo info) {
        checkExecutorThread();
        assertThat(request.isDone()).isTrue();
        // Should happen at most once for a single request.
        assertThat(mOnCanceledCalled).isFalse();
        assertThat(mOnErrorCalled).isFalse();
        assertThat(mError).isNull();

        mResponseStep = ResponseStep.ON_CANCELED;
        mResponseInfo = info;
        mOnCanceledCalled = true;
        openDone();
        mBlockOnTerminalState.block();
        maybeThrowCancelOrPause(request);
    }

    public void startNextRead(UrlRequest request) {
        startNextRead(request, ByteBuffer.allocateDirect(mReadBufferSize));
    }

    public void startNextRead(UrlRequest request, ByteBuffer buffer) {
        mBufferPositionBeforeRead = buffer.position();
        request.read(buffer);
    }

    public boolean isDone() {
        // It's not mentioned by the Android docs, but block(0) seems to block
        // indefinitely, so have to block for one millisecond to get state
        // without blocking.
        return mDone.block(1);
    }

    /**
     * Asserts that there is no callback error before trying to access responseInfo. Only use this
     * when you expect {@code mError} to be null.
     * @return {@link UrlResponseInfo}
     */
    public UrlResponseInfo getResponseInfoWithChecks() {
        assertThat(mError).isNull();
        assertThat(mOnErrorCalled).isFalse();
        assertThat(mResponseInfo).isNotNull();
        return mResponseInfo;
    }

    /**
     * Simply returns {@code mResponseInfo} with no nullability or error checks.
     * @return {@link UrlResponseInfo}
     */
    public UrlResponseInfo getResponseInfo() {
        return mResponseInfo;
    }

    protected void openDone() {
        mDone.open();
    }

    private void checkExecutorThread() {
        if (!mAllowDirectExecutor) {
            assertThat(Thread.currentThread()).isEqualTo(mExecutorThread);
        }
    }

    /**
     * Returns {@code false} if the listener should continue to advance the
     * request.
     */
    private boolean maybeThrowCancelOrPause(final UrlRequest request) {
        checkExecutorThread();
        if (mResponseStep != mFailureStep || mFailureType == FailureType.NONE) {
            if (!mAutoAdvance) {
                mStepBlock.open();
                return true;
            }
            return false;
        }

        if (mFailureType == FailureType.THROW_SYNC) {
            assertThat(mCallbackExceptionThrown).isFalse();
            mCallbackExceptionThrown = true;
            throw new IllegalStateException("Listener Exception.");
        }
        Runnable task =
                new Runnable() {
                    @Override
                    public void run() {
                        request.cancel();
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
     * A simple callback for a succeeding non-redirected request. Fails when other callback methods
     * that should not be executed are called.
     */
    public static class SimpleSucceedingCallback extends UrlRequest.Callback {
        public final ConditionVariable done = new ConditionVariable();
        private final ExecutorService mExecutor;

        public SimpleSucceedingCallback() {
            mExecutor = Executors.newSingleThreadExecutor();
        }

        @Override
        public void onRedirectReceived(UrlRequest request, UrlResponseInfo info, String location) {
            fail();
        }

        @Override
        public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
            request.read(ByteBuffer.allocateDirect(32 * 1024));
        }

        @Override
        public void onReadCompleted(
                UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
            byteBuffer.clear(); // we don't care about the data
            request.read(byteBuffer);
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            done.open();
        }

        @Override
        public void onCanceled(UrlRequest request, UrlResponseInfo info) {
            fail();
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException e) {
            fail(e.getMessage());
        }

        public ExecutorService getExecutor() {
            return mExecutor;
        }
    }
}
