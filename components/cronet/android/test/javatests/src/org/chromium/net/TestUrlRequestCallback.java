// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertNull;
import static junit.framework.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.assertContains;

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
    public UrlResponseInfo mResponseInfo;
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
            return new Thread(new Runnable() {
                @Override
                public void run() {
                    StrictMode.ThreadPolicy threadPolicy = StrictMode.getThreadPolicy();
                    try {
                        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
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

    /**
     * Set {@code mExecutorThread}.
     */
    private void fillInExecutorThread() {
        mExecutorService.execute(new Runnable() {
            @Override
            public void run() {
                mExecutorThread = Thread.currentThread();
            }
        });
    }

    /**
     * Create a {@link TestUrlRequestCallback} with a new single-threaded executor.
     */
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
            assertTrue("ExecutorService is interrupted while waiting for termination", false);
        }
        assertTrue(mExecutorService.isTerminated());
    }

    @Override
    public void onRedirectReceived(
            UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
        checkExecutorThread();
        assertFalse(request.isDone());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                || mResponseStep == ResponseStep.ON_RECEIVED_REDIRECT);
        assertNull(mError);

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
        assertFalse(request.isDone());
        assertTrue(mResponseStep == ResponseStep.NOTHING
                || mResponseStep == ResponseStep.ON_RECEIVED_REDIRECT);
        assertNull(mError);

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
        assertFalse(request.isDone());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                || mResponseStep == ResponseStep.ON_READ_COMPLETED);
        assertNull(mError);

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
        assertTrue(request.isDone());
        assertTrue(mResponseStep == ResponseStep.ON_RESPONSE_STARTED
                || mResponseStep == ResponseStep.ON_READ_COMPLETED);
        assertFalse(mOnErrorCalled);
        assertFalse(mOnCanceledCalled);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_SUCCEEDED;
        mResponseInfo = info;
        openDone();
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
        assertTrue(request.isDone());
        // Shouldn't happen after success.
        assertTrue(mResponseStep != ResponseStep.ON_SUCCEEDED);
        // Should happen at most once for a single request.
        assertFalse(mOnErrorCalled);
        assertFalse(mOnCanceledCalled);
        assertNull(mError);
        if (mCallbackExceptionThrown) {
            assertTrue(error instanceof CallbackException);
            assertContains("Exception received from UrlRequest.Callback", error.getMessage());
            assertNotNull(error.getCause());
            assertTrue(error.getCause() instanceof IllegalStateException);
            assertContains("Listener Exception.", error.getCause().getMessage());
        }

        mResponseStep = ResponseStep.ON_FAILED;
        mOnErrorCalled = true;
        mError = error;
        openDone();
        maybeThrowCancelOrPause(request);
    }

    @Override
    public void onCanceled(UrlRequest request, UrlResponseInfo info) {
        checkExecutorThread();
        assertTrue(request.isDone());
        // Should happen at most once for a single request.
        assertFalse(mOnCanceledCalled);
        assertFalse(mOnErrorCalled);
        assertNull(mError);

        mResponseStep = ResponseStep.ON_CANCELED;
        mOnCanceledCalled = true;
        openDone();
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

    protected void openDone() {
        mDone.open();
    }

    private void checkExecutorThread() {
        if (!mAllowDirectExecutor) {
            assertEquals(mExecutorThread, Thread.currentThread());
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
            assertFalse(mCallbackExceptionThrown);
            mCallbackExceptionThrown = true;
            throw new IllegalStateException("Listener Exception.");
        }
        Runnable task = new Runnable() {
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
}
