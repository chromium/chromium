// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.webshare;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.blink.mojom.Blob;
import org.chromium.blink.mojom.BlobReaderClient;
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

/** Receives a blob over mojom and writes it to the stream. */
public class BlobReceiver implements BlobReaderClient {
    private static final String TAG = "share";
    private static final int CHUNK_SIZE = 64 * 1024;
    private static final int PIPE_CAPACITY = 2 * CHUNK_SIZE;

    private final ByteBuffer mBuffer;
    private final OutputStream mOutputStream;
    private long mMaximumContentSize;
    private long mExpectedContentSize;
    private long mReceivedContentSize;
    private DataPipe.ConsumerHandle mConsumerHandle;
    private Callback<Integer> mCallback;

    /**
     * Constructs a BlobReceiver.
     *
     * @param outputStream the destination for the blob contents.
     * @param maximumContentSize the maximum permitted length of the blob.
     */
    public BlobReceiver(OutputStream outputStream, long maximumContentSize) {
        mBuffer = ByteBuffer.allocateDirect(CHUNK_SIZE);
        mOutputStream = outputStream;
        mMaximumContentSize = maximumContentSize;
    }

    /**
     * Initiates reading of the blob contents.
     *
     * @param blob the blob to read.
     * @param callback the callback to call when reading is complete.
     */
    public void start(Blob blob, Callback<Integer> callback) {
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
        if (mCallback == null) return;
        reportError(e.getMojoResult(), "Connection error detected.");
    }

    // BlobReaderClient
    @Override
    public void onCalculatedSize(long totalSize, long expectedContentSize) {
        if (mCallback == null) return;
        if (expectedContentSize > mMaximumContentSize) {
            reportError(MojoResult.RESOURCE_EXHAUSTED, "Stream exceeds permitted size");
            return;
        }
        mExpectedContentSize = expectedContentSize;
        if (mReceivedContentSize >= mExpectedContentSize) {
            complete();
            return;
        }

        Watcher watcher = CoreImpl.getInstance().getWatcher();
        watcher.start(
                mConsumerHandle,
                Core.HandleSignals.READABLE,
                new Watcher.Callback() {
                    @Override
                    public void onResult(int result) {
                        if (mCallback == null) return;
                        if (result == MojoResult.OK) {
                            read();
                        } else {
                            reportError(result, "Watcher reported error.");
                        }
                    }
                });
    }

    // BlobReaderClient
    @Override
    public void onComplete(int status, long dataLength) {
        if (mCallback == null) return;
        read();
    }

    private void read() {
        try {
            while (true) {
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

    private void complete() {
        try {
            mOutputStream.close();
        } catch (IOException e) {
            reportError(MojoResult.CANCELLED, "Failed to close stream.");
            return;
        }
        mCallback.onResult(MojoResult.OK);
        mCallback = null;
    }

    private void reportError(int result, String message) {
        if (result == MojoResult.OK) {
            result = MojoResult.INVALID_ARGUMENT;
        }
        Log.w(TAG, message);
        StreamUtil.closeQuietly(mOutputStream);
        mCallback.onResult(result);
        mCallback = null;
    }
}
