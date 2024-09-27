// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import androidx.annotation.IntDef;

import org.chromium.net.UrlRequest.Status;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

public class UrlRequestUtil {
    /** Possible URL Request statuses. */
    @IntDef({
        Status.INVALID,
        Status.IDLE,
        Status.WAITING_FOR_STALLED_SOCKET_POOL,
        Status.WAITING_FOR_AVAILABLE_SOCKET,
        Status.WAITING_FOR_DELEGATE,
        Status.WAITING_FOR_CACHE,
        Status.DOWNLOADING_PAC_FILE,
        Status.RESOLVING_PROXY_FOR_URL,
        Status.RESOLVING_HOST_IN_PAC_FILE,
        Status.ESTABLISHING_PROXY_TUNNEL,
        Status.RESOLVING_HOST,
        Status.CONNECTING,
        Status.SSL_HANDSHAKE,
        Status.SENDING_REQUEST,
        Status.WAITING_FOR_RESPONSE,
        Status.READING_RESPONSE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface StatusValues {}

    /**
     * Convert a LoadState int to one of values listed above.
     * @param loadState a LoadState to convert.
     * @return static int Status.
     */
    @StatusValues
    public static int convertLoadState(int loadState) {
        assert loadState >= LoadState.IDLE && loadState <= LoadState.READING_RESPONSE;
        switch (loadState) {
            case LoadState.IDLE:
                return Status.IDLE;

            case LoadState.WAITING_FOR_STALLED_SOCKET_POOL:
                return Status.WAITING_FOR_STALLED_SOCKET_POOL;

            case LoadState.WAITING_FOR_AVAILABLE_SOCKET:
                return Status.WAITING_FOR_AVAILABLE_SOCKET;

            case LoadState.WAITING_FOR_DELEGATE:
                return Status.WAITING_FOR_DELEGATE;

            case LoadState.WAITING_FOR_CACHE:
                return Status.WAITING_FOR_CACHE;

            case LoadState.DOWNLOADING_PAC_FILE:
                return Status.DOWNLOADING_PAC_FILE;

            case LoadState.RESOLVING_PROXY_FOR_URL:
                return Status.RESOLVING_PROXY_FOR_URL;

            case LoadState.RESOLVING_HOST_IN_PAC_FILE:
                return Status.RESOLVING_HOST_IN_PAC_FILE;

            case LoadState.ESTABLISHING_PROXY_TUNNEL:
                return Status.ESTABLISHING_PROXY_TUNNEL;

            case LoadState.RESOLVING_HOST:
                return Status.RESOLVING_HOST;

            case LoadState.CONNECTING:
                return Status.CONNECTING;

            case LoadState.SSL_HANDSHAKE:
                return Status.SSL_HANDSHAKE;

            case LoadState.SENDING_REQUEST:
                return Status.SENDING_REQUEST;

            case LoadState.WAITING_FOR_RESPONSE:
                return Status.WAITING_FOR_RESPONSE;

            case LoadState.READING_RESPONSE:
                return Status.READING_RESPONSE;

            default:
                // A load state is retrieved but there is no corresponding
                // request status. This most likely means that the mapping is
                // incorrect.
                throw new IllegalArgumentException("No request status found.");
        }
    }
}
