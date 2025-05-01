// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.ConnectivityManager;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.IOException;

// Implementation for calling ConnectivityManager hidden APIs, which are not available outside the
// Android Connectivity mainline module.
// Note that this code is only built when Cronet is built as part of the Android platform.
public class ConnectivityManagerShim {
    private static final String TAG = ConnectivityManagerShim.class.getSimpleName();

    public static void registerQuicConnectionClosePayload(
            final ConnectivityManager cm, final int socket, final byte[] payload) {
        try (final ParcelFileDescriptor pfd = ParcelFileDescriptor.fromFd(socket)) {
            cm.registerQuicConnectionClosePayload(pfd, payload);
        } catch (IOException e) {
            Log.w(TAG, "Failed to register QUIC connection close payload: " + e);
        }
    }

    public static void unregisterQuicConnectionClosePayload(
            final ConnectivityManager cm, final int socket) {
        try (final ParcelFileDescriptor pfd = ParcelFileDescriptor.fromFd(socket)) {
            cm.unregisterQuicConnectionClosePayload(pfd);
        } catch (IOException e) {
            Log.w(TAG, "Failed to unregister QUIC connection close payload: " + e);
        }
    }
}
