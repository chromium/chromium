// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.IntDef;

import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;
import org.chromium.net.impl.JavaUrlRequestUtils.CheckedRunnable;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.concurrent.Executor;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Base class for Java UrlRequest implementations of UploadDataSink. Handles asynchronicity and
 * manages the executors for this upload.
 */
public abstract class JavaUploadDataSinkBase extends UploadDataSink {
    @IntDef({
        SinkState.AWAITING_READ_RESULT,
        SinkState.AWAITING_REWIND_RESULT,
        SinkState.UPLOADING,
        SinkState.NOT_STARTED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface SinkState {
        int AWAITING_READ_RESULT = 0;
        int AWAITING_REWIND_RESULT = 1;
        int UPLOADING = 2;
        int NOT_STARTED = 3;
    }

    public static final int DEFAULT_UPLOAD_BUFFER_SIZE = 8192;

    private final AtomicInteger /*SinkState*/ mSinkState = new AtomicInteger(SinkState.NOT_STARTED);
    private final Executor mUserUploadExecutor;
    private final Executor mExecutor;
    private final VersionSafeCallbacks.UploadDataProviderWrapper mUploadProvider;
    private ByteBuffer mBuffer;

    /** This holds the total bytes to send (the content-length). -1 if unknown. */
    private long mTotalBytes;

    /** This holds the bytes written so far */
    private long mWrittenBytes;

    private int mReadCount;

    public JavaUploadDataSinkBase(
            final Executor userExecutor, Executor executor, UploadDataProvider provider) {
        mUserUploadExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable runnable) {
                        try {
                            userExecutor.execute(runnable);
                        } catch (RejectedExecutionException e) {
                            processUploadError(e);
                        }
                    }
                };
        mExecutor = executor;
        mUploadProvider = new VersionSafeCallbacks.UploadDataProviderWrapper(provider);
    }

    @Override
    public void onReadSucceeded(final boolean finalChunk) {
        if (!mSinkState.compareAndSet(
                /* expected= */ SinkState.AWAITING_READ_RESULT,
                /* updated= */ SinkState.UPLOADING)) {
            throw new IllegalStateException(
                    "onReadSucceeded() called when not awaiting a read result; in state: "
                            + mSinkState.get());
        }
        JavaUrlRequestUtils.CheckedRunnable checkedRunnable =
                () -> {
                    mBuffer.flip();
                    if (mTotalBytes != -1 && mTotalBytes - mWrittenBytes < mBuffer.remaining()) {
                        String msg =
                                String.format(
                                        Locale.getDefault(),
                                        "Read upload data length %d exceeds expected length %d",
                                        mWrittenBytes + mBuffer.remaining(),
                                        mTotalBytes);
                        processUploadError(new IllegalArgumentException(msg));
                        return;
                    }

                    if (mBuffer.remaining() == 0 && !finalChunk) {
                        // Sending an empty buffer through the fallback implementation
                        // leads to successful requests but in order to consolidate the fallback
                        // implementation with the native implementation, it was decided
                        // to make uploading an empty buffer an illegal operation that leads
                        // to a failed request with upload error.
                        //
                        // See b/332860415 for more details.
                        processUploadError(
                                new IllegalStateException(
                                        "Bytes read can't be zero except for last chunk!"));
                        return;
                    }

                    mWrittenBytes += processSuccessfulRead(mBuffer);

                    if (mWrittenBytes < mTotalBytes || (mTotalBytes == -1 && !finalChunk)) {
                        mBuffer.clear();
                        mSinkState.set(SinkState.AWAITING_READ_RESULT);
                        readFromProvider();
                    } else if (mTotalBytes == -1) {
                        finish();
                    } else if (mTotalBytes == mWrittenBytes) {
                        finish();
                    } else {
                        String msg =
                                String.format(
                                        Locale.getDefault(),
                                        "Read upload data length %d exceeds expected length %d",
                                        mWrittenBytes,
                                        mTotalBytes);
                        processUploadError(new IllegalArgumentException(msg));
                    }
                };
        mExecutor.execute(getErrorSettingRunnable(checkedRunnable));
    }

    @Override
    public void onRewindSucceeded() {
        if (!mSinkState.compareAndSet(
                /* expected= */ SinkState.AWAITING_REWIND_RESULT,
                /* updated= */ SinkState.UPLOADING)) {
            throw new IllegalStateException(
                    "onRewindSucceeded() called when not awaiting a rewind; in state: "
                            + mSinkState.get());
        }
        startRead();
    }

    @Override
    public void onReadError(Exception exception) {
        processUploadError(exception);
    }

    @Override
    public void onRewindError(Exception exception) {
        processUploadError(exception);
    }

    private void startRead() {
        mExecutor.execute(
                getErrorSettingRunnable(
                        () -> {
                            initializeRead();
                            mSinkState.set(SinkState.AWAITING_READ_RESULT);
                            readFromProvider();
                        }));
    }

    private void readFromProvider() {
        executeOnUploadExecutor(
                () -> {
                    mUploadProvider.read(JavaUploadDataSinkBase.this, mBuffer);
                    // Increment the read count on the internal executor, which is serialized, to
                    // prevent potential races with reader code.
                    mExecutor.execute(
                            () -> {
                                mReadCount++;
                            });
                });
    }

    /**
     * Helper method to execute a checked runnable on the upload executor and process any errors
     * that occur as upload errors.
     *
     * @param runnable the runnable to attempt to run and check for errors
     */
    private void executeOnUploadExecutor(JavaUrlRequestUtils.CheckedRunnable runnable) {
        try {
            mUserUploadExecutor.execute(getUploadErrorSettingRunnable(runnable));
        } catch (RejectedExecutionException e) {
            processUploadError(e);
        }
    }

    /**
     * Starts the upload. This method can be called multiple times. If it is not the first time it
     * is called the {@link UploadDataProvider} must rewind.
     *
     * @param firstTime true if this is the first time this {@link UploadDataSink} has started an
     *                  upload
     */
    public void start(final boolean firstTime) {
        executeOnUploadExecutor(
                () -> {
                    mTotalBytes = mUploadProvider.getLength();
                    if (mTotalBytes == 0) {
                        finish();
                    } else {
                        // If we know how much data we have to upload, and it's small, we can
                        // save memory by allocating a reasonably sized buffer to read into.
                        if (mTotalBytes > 0 && mTotalBytes < DEFAULT_UPLOAD_BUFFER_SIZE) {
                            // Allocate one byte more than necessary, to detect callers
                            // uploading more bytes than they specified in length.
                            mBuffer = ByteBuffer.allocateDirect((int) mTotalBytes + 1);
                        } else {
                            mBuffer = ByteBuffer.allocateDirect(DEFAULT_UPLOAD_BUFFER_SIZE);
                        }

                        initializeStart(mTotalBytes);

                        if (firstTime) {
                            startRead();
                        } else {
                            mSinkState.set(SinkState.AWAITING_REWIND_RESULT);
                            mUploadProvider.rewind(JavaUploadDataSinkBase.this);
                        }
                    }
                });
    }

    /**
     * Returns the number of times {@link UploadDataProvider#read} returned successfully.
     *
     * <p>Thread safety: only safe to call from the internal executor.
     */
    int getReadCount() {
        return mReadCount;
    }

    /**
     * Gets a runnable that checks for errors and processes them by setting an error state when
     * executing a {@link CheckedRunnable}.
     *
     * @param runnable The runnable to run.
     * @return a runnable that checks for errors
     */
    protected abstract Runnable getErrorSettingRunnable(
            JavaUrlRequestUtils.CheckedRunnable runnable);

    /**
     * Gets a runnable that checks for errors and processes them by setting an upload error state
     * when executing a {@link CheckedRunnable}.
     *
     * @param runnable The runnable to run.
     * @return a runnable that checks for errors
     */
    protected abstract Runnable getUploadErrorSettingRunnable(
            JavaUrlRequestUtils.CheckedRunnable runnable);

    /**
     * Processes an error encountered while uploading data.
     *
     * @param error the {@link Throwable} to process
     */
    protected abstract void processUploadError(final Throwable error);

    /**
     * Called when a successful read has occurred and there is new data in the {@code mBuffer} to
     * process.
     *
     * @return the number of bytes processed in this read
     */
    protected abstract int processSuccessfulRead(ByteBuffer buffer) throws IOException;

    /** Finishes this upload. Called when the upload is complete. */
    protected abstract void finish() throws IOException;

    /**
     * Initializes the {@link UploadDataSink} before each call to {@code read} in the {@link
     * UploadDataProvider}.
     */
    protected abstract void initializeRead() throws IOException;

    /**
     * Initializes the {@link UploadDataSink} at the start of the upload.
     *
     * @param totalBytes the total number of bytes to be retrieved in this upload
     */
    protected abstract void initializeStart(long totalBytes);
}
