// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

/** Makes {@link NativeTestServer} connections stop answering. */
public final class DroppingConnectionCallback implements NativeTestServer.ConnectionCallback {
    /** Which connections stop answering, and from when. */
    public enum Drop {
        /** Every connection, from the TLS ClientHello on. */
        ALL_BEFORE_TLS,
        /** The first connection, from the TLS ClientHello on. */
        FIRST_BEFORE_TLS,
        /**
         * The first connection, once the client has finished its side of the TLS handshake. The
         * server sends its first TLS 1.3 flight, then stops reading, so it drops the client's
         * Finished message and everything after it.
         */
        FIRST_AFTER_TLS,
    }

    private final Drop mDrop;
    // Only used on the server's IO thread.
    private boolean mWroteToFirstConnection;

    public DroppingConnectionCallback(Drop drop) {
        mDrop = drop;
    }

    @Override
    public boolean shouldRead(int connectionIndex) {
        switch (mDrop) {
            case ALL_BEFORE_TLS:
                return false;
            case FIRST_BEFORE_TLS:
                return connectionIndex != 0;
            case FIRST_AFTER_TLS:
                return connectionIndex != 0 || !mWroteToFirstConnection;
        }
        throw new AssertionError();
    }

    @Override
    public boolean shouldWrite(int connectionIndex) {
        if (connectionIndex == 0) {
            mWroteToFirstConnection = true;
        }
        return true;
    }
}
