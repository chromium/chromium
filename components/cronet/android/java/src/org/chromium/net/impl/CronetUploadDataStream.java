// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.annotation.SuppressLint;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UploadDataSink;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.util.concurrent.Executor;

import javax.annotation.concurrent.GuardedBy;

/**
 * CronetUploadDataStream handles communication between an upload body
 * encapsulated in the embedder's {@link UploadDataSink} and a C++
 * UploadDataStreamAdapter, which it owns. It's attached to a {@link
 * CronetUrlRequest}'s during the construction of request's native C++ objects
 * on the network thread, though it's created on one of the embedder's threads.
 * It is called by the UploadDataStreamAdapter on the network thread, but calls
 * into the UploadDataSink and the UploadDataStreamAdapter on the Executor
 * passed into its constructor.
 */
@JNINamespace("cronet")
@VisibleForTesting
public final class CronetUploadDataStream extends UploadDataSink {
    private static final String TAG = CronetUploadDataStream.class.getSimpleName();
    // These are never changed, once a request starts.
    private final Executor mExecutor;
    private final VersionSafeCallbacks.UploadDataProviderWrapper mDataProvider;
    private final CronetUrlRequest mRequest;
    private long mLength;
    private long mRemainingLength;
    private long mByteBufferLimit;

    // Reusable read task, to reduce redundant memory allocation.
    private final Runnable mReadTask = new Runnable() {
        @Override
        public void run() {
            synchronized (mLock) {
                if (mUploadDataStreamAdapter == 0) {
                    return;
                }
                checkState(UserCallback.NOT_IN_CALLBACK);
                if (mByteBuffer == null) {
                    throw new IllegalStateException("Unexpected readData call. Buffer is null");
                }
                mInWhichUserCallback = UserCallback.READ;
            }
            try {
                checkCallingThread();
                assert mByteBuffer.position() == 0;
                mDataProvider.read(CronetUploadDataStream.this, mByteBuffer);
            } catch (Exception exception) {
                onError(exception);
            }
        }
    };

    // ByteBuffer created in the native code and passed to
    // UploadDataProvider for reading. It is only valid from the
    // call to mDataProvider.read until onError or onReadSucceeded.
    private ByteBuffer mByteBuffer;

    // Lock that protects all subsequent variables. The adapter has to be
    // protected to ensure safe shutdown, mReading and mRewinding are protected
    // to robustly detect getting read/rewind results more often than expected.
    private final Object mLock = new Object();

    // Native adapter object, owned by the CronetUploadDataStream. It's only
    // deleted after the native UploadDataStream object is destroyed. All access
    // to the adapter is synchronized, for safe usage and cleanup.
    @GuardedBy("mLock")
    private long mUploadDataStreamAdapter;

    @IntDef({UserCallback.READ, UserCallback.REWIND, UserCallback.GET_LENGTH,
            UserCallback.NOT_IN_CALLBACK})
    @Retention(RetentionPolicy.SOURCE)
    private @interface UserCallback {
        int READ = 0;
        int REWIND = 1;
        int GET_LENGTH = 2;
        int NOT_IN_CALLBACK = 3;
    }

    @GuardedBy("mLock")
    private @UserCallback int mInWhichUserCallback = UserCallback.NOT_IN_CALLBACK;
    @GuardedBy("mLock")
    private boolean mDestroyAdapterPostponed;
    private Runnable mOnDestroyedCallbackForTesting;

    /**
     * Constructs a CronetUploadDataStream.
     * @param dataProvider the UploadDataProvider to read data from.
     * @param executor the Executor to execute UploadDataProvider tasks.
     */
    public CronetUploadDataStream(
            UploadDataProvider dataProvider, Executor executor, CronetUrlRequest request) {
        mExecutor = executor;
        mDataProvider = new VersionSafeCallbacks.UploadDataProviderWrapper(dataProvider);
        mRequest = request;
    }

    /**
     * Called by native code to make the UploadDataProvider read data into
     * {@code byteBuffer}.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void readData(ByteBuffer byteBuffer) {
        mByteBuffer = byteBuffer;
        mByteBufferLimit = byteBuffer.limit();
        postTaskToExecutor(mReadTask);
    }

    // TODO(mmenke): Consider implementing a cancel method.
    // currently wait for any pending read to complete.

    /**
     * Called by native code to make the UploadDataProvider rewind upload data.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void rewind() {
        Runnable task = new Runnable() {
            @Override
            public void run() {
                synchronized (mLock) {
                    if (mUploadDataStreamAdapter == 0) {
                        return;
                    }
                    checkState(UserCallback.NOT_IN_CALLBACK);
                    mInWhichUserCallback = UserCallback.REWIND;
                }
                try {
                    checkCallingThread();
                    mDataProvider.rewind(CronetUploadDataStream.this);
                } catch (Exception exception) {
                    onError(exception);
                }
            }
        };
        postTaskToExecutor(task);
    }

    private void checkCallingThread() {
        mRequest.checkCallingThread();
    }

    @GuardedBy("mLock")
    private void checkState(@UserCallback int mode) {
        if (mInWhichUserCallback != mode) {
            throw new IllegalStateException(
                    "Expected " + mode + ", but was " + mInWhichUserCallback);
        }
    }

    /**
     * Called when the native UploadDataStream is destroyed.  At this point,
     * the native adapter needs to be destroyed, but only after any pending
     * read operation completes, as the adapter owns the read buffer.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    void onUploadDataStreamDestroyed() {
        destroyAdapter();
    }

    /**
     * Helper method called when an exception occurred. This method resets
     * states and propagates the error to the request.
     */
    private void onError(Throwable exception) {
        final boolean sendClose;
        synchronized (mLock) {
            if (mInWhichUserCallback == UserCallback.NOT_IN_CALLBACK) {
                throw new IllegalStateException(
                        "There is no read or rewind or length check in progress.");
            }
            sendClose = mInWhichUserCallback == UserCallback.GET_LENGTH;
            mInWhichUserCallback = UserCallback.NOT_IN_CALLBACK;
            mByteBuffer = null;
            destroyAdapterIfPostponed();
        }
        // Failure before length is obtained means that the request has failed before the
        // adapter has been initialized. Close the UploadDataProvider. This is safe to call
        // here since failure during getLength can only happen on the user's executor.
        if (sendClose) {
            try {
                mDataProvider.close();
            } catch (Exception e) {
                Log.e(TAG, "Failure closing data provider", e);
            }
        }

        // Just fail the request - simpler to fail directly, and
        // UploadDataStream only supports failing during initialization, not
        // while reading. The request is smart enough to handle the case where
        // it was already canceled by the embedder.
        mRequest.onUploadException(exception);
    }

    @Override
    @SuppressLint("DefaultLocale")
    public void onReadSucceeded(boolean lastChunk) {
        synchronized (mLock) {
            checkState(UserCallback.READ);
            if (mByteBufferLimit != mByteBuffer.limit()) {
                throw new IllegalStateException("ByteBuffer limit changed");
            }
            if (lastChunk && mLength >= 0) {
                throw new IllegalArgumentException("Non-chunked upload can't have last chunk");
            }
            int bytesRead = mByteBuffer.position();
            mRemainingLength -= bytesRead;
            if (mRemainingLength < 0 && mLength >= 0) {
                throw new IllegalArgumentException(
                        String.format("Read upload data length %d exceeds expected length %d",
                                mLength - mRemainingLength, mLength));
            }
            mByteBuffer.position(0);
            mByteBuffer = null;
            mInWhichUserCallback = UserCallback.NOT_IN_CALLBACK;

            destroyAdapterIfPostponed();
            // Request may been canceled already.
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            CronetUploadDataStreamJni.get().onReadSucceeded(
                    mUploadDataStreamAdapter, CronetUploadDataStream.this, bytesRead, lastChunk);
        }
    }

    @Override
    public void onReadError(Exception exception) {
        synchronized (mLock) {
            checkState(UserCallback.READ);
            onError(exception);
        }
    }

    @Override
    public void onRewindSucceeded() {
        synchronized (mLock) {
            checkState(UserCallback.REWIND);
            mInWhichUserCallback = UserCallback.NOT_IN_CALLBACK;
            mRemainingLength = mLength;
            // Request may been canceled already.
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            CronetUploadDataStreamJni.get().onRewindSucceeded(
                    mUploadDataStreamAdapter, CronetUploadDataStream.this);
        }
    }

    @Override
    public void onRewindError(Exception exception) {
        synchronized (mLock) {
            checkState(UserCallback.REWIND);
            onError(exception);
        }
    }

    /**
     * Posts task to application Executor.
     */
    void postTaskToExecutor(Runnable task) {
        try {
            mExecutor.execute(task);
        } catch (Throwable e) {
            // Just fail the request. The request is smart enough to handle the
            // case where it was already canceled by the embedder.
            mRequest.onUploadException(e);
        }
    }

    /**
     * The adapter is owned by the CronetUploadDataStream, so it can be
     * destroyed safely when there is no pending read; however, destruction is
     * initiated by the destruction of the native UploadDataStream.
     */
    private void destroyAdapter() {
        synchronized (mLock) {
            if (mInWhichUserCallback == UserCallback.READ) {
                // Wait for the read to complete before destroy the adapter.
                mDestroyAdapterPostponed = true;
                return;
            }
            if (mUploadDataStreamAdapter == 0) {
                return;
            }
            CronetUploadDataStreamJni.get().destroy(mUploadDataStreamAdapter);
            mUploadDataStreamAdapter = 0;
            if (mOnDestroyedCallbackForTesting != null) {
                mOnDestroyedCallbackForTesting.run();
            }
        }
        postTaskToExecutor(new Runnable() {
            @Override
            public void run() {
                try {
                    checkCallingThread();
                    mDataProvider.close();
                } catch (Exception e) {
                    Log.e(TAG, "Exception thrown when closing", e);
                }
            }
        });
    }

    /**
     * Destroys the native adapter if the destruction is postponed due to a
     * pending read, which has since completed. Caller needs to be on executor
     * thread.
     */
    private void destroyAdapterIfPostponed() {
        synchronized (mLock) {
            if (mInWhichUserCallback == UserCallback.READ) {
                throw new IllegalStateException(
                        "Method should not be called when read has not completed.");
            }
            if (mDestroyAdapterPostponed) {
                destroyAdapter();
            }
        }
    }

    /**
     * Initializes upload length by getting it from data provider. Submits to
     * the user's executor thread to allow getLength() to block and/or report errors.
     * If data provider throws an exception, then it is reported to the request.
     * No native calls to urlRequest are allowed as this is done before request
     * start, so native object may not exist.
     */
    void initializeWithRequest() {
        synchronized (mLock) {
            mInWhichUserCallback = UserCallback.GET_LENGTH;
        }
        try {
            mRequest.checkCallingThread();
            mLength = mDataProvider.getLength();
            mRemainingLength = mLength;
        } catch (Throwable t) {
            onError(t);
        }
        synchronized (mLock) {
            mInWhichUserCallback = UserCallback.NOT_IN_CALLBACK;
        }
    }

    /**
     * Creates native objects and attaches them to the underlying request
     * adapter object. Always called on executor thread.
     */
    void attachNativeAdapterToRequest(final long requestAdapter) {
        synchronized (mLock) {
            mUploadDataStreamAdapter = CronetUploadDataStreamJni.get().attachUploadDataToRequest(
                    CronetUploadDataStream.this, requestAdapter, mLength);
        }
    }

    /**
     * Creates a native CronetUploadDataStreamAdapter and
     * CronetUploadDataStream for testing.
     * @return the address of the native CronetUploadDataStream object.
     */
    @VisibleForTesting
    public long createUploadDataStreamForTesting() throws IOException {
        synchronized (mLock) {
            mUploadDataStreamAdapter = CronetUploadDataStreamJni.get().createAdapterForTesting(
                    CronetUploadDataStream.this);
            mLength = mDataProvider.getLength();
            mRemainingLength = mLength;
            return CronetUploadDataStreamJni.get().createUploadDataStreamForTesting(
                    CronetUploadDataStream.this, mLength, mUploadDataStreamAdapter);
        }
    }

    @VisibleForTesting
    void setOnDestroyedCallbackForTesting(Runnable onDestroyedCallbackForTesting) {
        mOnDestroyedCallbackForTesting = onDestroyedCallbackForTesting;
    }

    // Native methods are implemented in upload_data_stream_adapter.cc.
    @NativeMethods
    interface Natives {
        long attachUploadDataToRequest(
                CronetUploadDataStream caller, long urlRequestAdapter, long length);

        long createAdapterForTesting(CronetUploadDataStream caller);
        long createUploadDataStreamForTesting(
                CronetUploadDataStream caller, long length, long adapter);
        @NativeClassQualifiedName("CronetUploadDataStreamAdapter")
        void onReadSucceeded(
                long nativePtr, CronetUploadDataStream caller, int bytesRead, boolean finalChunk);

        @NativeClassQualifiedName("CronetUploadDataStreamAdapter")
        void onRewindSucceeded(long nativePtr, CronetUploadDataStream caller);

        @NativeClassQualifiedName("CronetUploadDataStreamAdapter")
        void destroy(long nativePtr);
    }
}
