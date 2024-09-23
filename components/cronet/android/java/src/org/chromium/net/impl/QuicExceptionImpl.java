// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.ConnectionCloseSource;
import org.chromium.net.QuicException;

/** Implements {@link QuicException}. */
public class QuicExceptionImpl extends QuicException {
    private final int mQuicDetailedErrorCode;
    private final @ConnectionCloseSource int mSource;
    private final NetworkExceptionImpl mNetworkException;

    /** Deprecated, maintained for backward compatibility. */
    @Deprecated
    public QuicExceptionImpl(
            String message, int errorCode, int netErrorCode, int quicDetailedErrorCode) {
        this(
                message,
                errorCode,
                netErrorCode,
                quicDetailedErrorCode,
                ConnectionCloseSource.UNKNOWN);
    }

    /**
     * Constructs an exception with a specific error.
     *
     * @param message explanation of failure.
     * @param netErrorCode Error code from <a
     *     href=https://chromium.googlesource.com/chromium/src/+/main/net/base/net_error_list.h>this
     *     list</a>.
     * @param quicDetailedErrorCode Detailed <a href="https://www.chromium.org/quic">QUIC</a> error
     *     code from <a href="https://cs.chromium.org/search/?q=symbol:%5CbQuicErrorCode%5Cb">
     *     QuicErrorCode</a>.
     * @param source Defines the initiator of the error code, See <a
     *     href="https://cs.chromium.org/search/?q=symbol:ConnectionCloseSource">
     *     ConnectionCloseSource</a> for more information.
     */
    public QuicExceptionImpl(
            String message,
            int errorCode,
            int netErrorCode,
            int quicDetailedErrorCode,
            @ConnectionCloseSource int source) {
        super(message, null);
        mNetworkException = new NetworkExceptionImpl(message, errorCode, netErrorCode);
        mQuicDetailedErrorCode = quicDetailedErrorCode;
        mSource = source;
    }

    @Override
    public String getMessage() {
        StringBuilder b = new StringBuilder(mNetworkException.getMessage());
        b.append(", QuicDetailedErrorCode=").append(mQuicDetailedErrorCode);
        b.append(", Source=").append(mSource);
        return b.toString();
    }

    @Override
    public int getErrorCode() {
        return mNetworkException.getErrorCode();
    }

    @Override
    public int getCronetInternalErrorCode() {
        return mNetworkException.getCronetInternalErrorCode();
    }

    @Override
    public boolean immediatelyRetryable() {
        return mNetworkException.immediatelyRetryable();
    }

    @Override
    public int getQuicDetailedErrorCode() {
        return mQuicDetailedErrorCode;
    }

    @Override
    public @ConnectionCloseSource int getConnectionCloseSource() {
        return mSource;
    }
}
