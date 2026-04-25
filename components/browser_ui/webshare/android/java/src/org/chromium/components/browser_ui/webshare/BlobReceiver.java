// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.webshare;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.Blob;
import org.chromium.blink.mojom.BlobReaderClient;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.DataPipe;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.MojoResult;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.ResultAnd;
import org.chromium.mojo.system.Watcher;
import org.chromium.mojo.system.impl.CoreImpl;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;

/**
 * Receives a blob over mojom and writes it to the stream. Even though it starts working on whatever
 * thread it is created on, the final callback is run on the UI thread, while file operations work
 * in the background thread.
 */
@NullMarked
public class BlobReceiver implements BlobReaderClient {
    private static final String TAG = "share";
    private static final int CHUNK_SIZE = 64 * 1024;
    private static final int PIPE_CAPACITY = 2 * CHUNK_SIZE;

    private final ByteBuffer mBuffer;
    private final OutputStream mOutputStream;
    private final long mMaximumContentSize;
    private long mExpectedContentSize;
    private long mReceivedContentSize;
    private volatile boolean mIsClosed;
    private DataPipe.@Nullable ConsumerHandle mConsumerHandle;
    private volatile @Nullable Callback<Integer> mCallback;
    private @Nullable Blob mBlob;
    private final TaskRunner mTaskRunner;

    /**
     * Constructs a BlobReceiver.
     *
     * @param outputStream the destination for the blob contents.
     * @param maximumContentSize the maximum permitted length of the blob.
     * @param taskRunner the task runner for background execution.
     */
    public BlobReceiver(OutputStream outputStream, long maximumContentSize, TaskRunner taskRunner) {
        mBuffer = ByteBuffer.allocateDirect(CHUNK_SIZE);
        mOutputStream = outputStream;
        mMaximumContentSize = maximumContentSize;
        mTaskRunner = taskRunner;
    }

    /**
     * Initiates reading of the blob contents.
     *
     * @param blob the blob to read.
     * @param callback the callback to call when reading is complete.
     */
    public void start(Blob blob, Callback<Integer> callback) {
        assert mBlob == null;
        assert mConsumerHandle == null;
        mBlob = blob;
        mCallback = callback;
        DataPipe.CreateOptions options = new DataPipe.CreateOptions();
        options.setElementNumBytes(1);
        options.setCapacityNumBytes(PIPE_CAPACITY);

        Pair<DataPipe.ProducerHandle, DataPipe.ConsumerHandle> pipe =
                CoreImpl.getInstance().createDataPipe(options);
        mConsumerHandle = pipe.second;
        blob.readAll(pipe.first, this);
    }

    // Interface
    @Override
    public void close() {}

    // ConnectionErrorHandler
    @Override
    public void onConnectionError(MojoException e) {
        mTaskRunner.execute(
                () -> {
                    if (mIsClosed) return;
                    assumeNonNull(mCallback);
                    reportError(e.getMojoResult(), "Connection error detected.");
                });
    }

    // BlobReaderClient
    @Override
    public void onCalculatedSize(long totalSize, long expectedContentSize) {
        mTaskRunner.execute(
                () -> {
                    if (mIsClosed) return;
                    assumeNonNull(mCallback);
                    if (expectedContentSize > mMaximumContentSize) {
                        reportError(MojoResult.RESOURCE_EXHAUSTED, "Stream exceeds permitted size");
                        return;
                    }
                    mExpectedContentSize = expectedContentSize;
                    if (mReceivedContentSize >= mExpectedContentSize) {
                        complete();
                        return;
                    }
                    final DataPipe.ConsumerHandle handle = mConsumerHandle;
                    assumeNonNull(handle);
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () -> {
                                startWatcher(handle);
                            });
                });
    }

    private void startWatcher(final DataPipe.ConsumerHandle consumerHandle) {
        if (mIsClosed) return;
        assumeNonNull(mCallback);
        Watcher watcher = CoreImpl.getInstance().getWatcher();
        assumeNonNull(consumerHandle);
        watcher.start(
                consumerHandle,
                Core.HandleSignals.READABLE,
                new Watcher.Callback() {
                    @Override
                    public void onResult(int result) {
                        mTaskRunner.execute(
                                () -> {
                                    if (mIsClosed) {
                                        return;
                                    }
                                    if (result == MojoResult.OK) {
                                        read();
                                    } else {
                                        reportError(result, "Watcher reported error.");
                                    }
                                });
                    }
                });
    }

    // BlobReaderClient
    @Override
    public void onComplete(int status, long dataLength) {
        mTaskRunner.execute(
                () -> {
                    if (mIsClosed) return;
                    assumeNonNull(mCallback);
                    read();
                });
    }

    private void read() {
        try {
            while (true) {
                assumeNonNull(mConsumerHandle);
                ResultAnd<Integer> result =
                        mConsumerHandle.readData(mBuffer, DataPipe.ReadFlags.NONE);

                if (result.getMojoResult() == MojoResult.SHOULD_WAIT) return;

                if (result.getMojoResult() != MojoResult.OK) {
                    reportError(result.getMojoResult(), "Failed to read from blob.");
                    return;
                }

                Integer bytesRead = result.getValue();
                if (bytesRead <= 0) {
                    reportError(MojoResult.SHOULD_WAIT, "No data available");
                    return;
                }
                try {
                    mOutputStream.write(mBuffer.array(), mBuffer.arrayOffset(), bytesRead);
                } catch (IOException e) {
                    reportError(MojoResult.DATA_LOSS, "Failed to write to stream.");
                    return;
                }
                mReceivedContentSize += bytesRead;
                if (mReceivedContentSize >= mExpectedContentSize) {
                    if (mReceivedContentSize == mExpectedContentSize) {
                        complete();
                    } else {
                        reportError(
                                MojoResult.OUT_OF_RANGE, "Received more bytes than expected size.");
                    }
                    return;
                }
            }
        } catch (MojoException e) {
            reportError(e.getMojoResult(), "Failed to receive blob.");
        }
    }

    // All functions below are called only from the task runner thread, with the callback and the
    // blob passed to the UI thread for running.
    private void complete() {
        try {
            mOutputStream.close();
        } catch (IOException e) {
            reportError(MojoResult.CANCELLED, "Failed to close stream.");
            return;
        }
        invokeCallbackAndCloseMojoEndpoints(MojoResult.OK);
    }

    private void reportError(int result, String message) {
        if (result == MojoResult.OK) {
            result = MojoResult.INVALID_ARGUMENT;
        }
        Log.w(TAG, message);
        StreamUtil.closeQuietly(mOutputStream);
        invokeCallbackAndCloseMojoEndpoints(result);
    }

    private void invokeCallbackAndCloseMojoEndpoints(int result) {
        mIsClosed = true;
        final DataPipe.ConsumerHandle handleToDestroy = mConsumerHandle;
        mConsumerHandle = null;

        final Blob blobToClose = mBlob;
        mBlob = null;

        assumeNonNull(mCallback);
        final Callback<Integer> callback = mCallback;
        mCallback = null;

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Close the consumer handle before callbacks to ensure native resources
                    // are freed before the callback potentially destroys the session.
                    if (handleToDestroy != null) {
                        handleToDestroy.close();
                    }
                    // The callback from the SharedFileCollator and the blob HAS
                    // to be run and destroyed on the UI thread, as both are not
                    // thread safe.
                    if (blobToClose != null) {
                        blobToClose.close();
                    }
                    callback.onResult(result);
                });
    }
}
