// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import org.chromium.net.UploadDataProvider;

import java.io.IOException;
import java.io.OutputStream;

/**
 * An abstract class of {@link OutputStream} that concrete implementations must
 * extend in order to be used in {@link CronetHttpURLConnection}.
 */
abstract class CronetOutputStream extends OutputStream {
    private IOException mException;
    private boolean mClosed;
    private boolean mRequestCompleted;

    @Override
    public void close() throws IOException {
        mClosed = true;
    }

    /**
     * Informs the underlying implementation that the user wishes to connect.
     *
     * <p>If this returns true, the request will be sent immediately upon return.
     *
     * <p>If this returns false, the request will not be sent. The implementation becomes
     * responsible for sending the request once ready to do so, by calling {@link
     * CronetHttpURLConnection#connect()}.
     *
     * <p>Note this MUST return true if the stream is closed.
     */
    abstract boolean connectRequested() throws IOException;

    /**
     * Checks whether content received is less than Content-Length. Used in {@link
     * CronetHttpURLConnection}.
     */
    abstract void checkReceivedEnoughContent() throws IOException;

    /** Returns {@link UploadDataProvider} implementation. */
    abstract UploadDataProvider getUploadDataProvider();

    /**
     * Signals that the request is done. If there is no error,
     * {@code exception} is null. Used by {@link CronetHttpURLConnection}.
     */
    void setRequestCompleted(IOException exception) {
        mException = exception;
        mRequestCompleted = true;
    }

    protected boolean isClosed() {
        return mClosed;
    }

    /** Throws an IOException if the stream is closed or the request is done. */
    protected void checkNotClosed() throws IOException {
        if (mRequestCompleted) {
            checkNoException();
            throw new IOException("Writing after request completed.");
        }
        if (mClosed) {
            throw new IOException("Stream has been closed.");
        }
    }

    /**
     * Throws the same IOException that the request is failed with. If there
     * is no exception reported, this method is no-op.
     */
    protected void checkNoException() throws IOException {
        if (mException != null) {
            throw mException;
        }
    }
}
