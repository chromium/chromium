// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.util.Log;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.VersionSafeCallbacks.RequestFinishedInfoListener;

import java.util.Collection;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidRequestFinishedInfoWrapper extends RequestFinishedInfoImpl {
    private static final String TAG = RequestFinishedInfoImpl.class.getSimpleName();

    private static boolean sListenerSupportLimitedLogged;

    private AndroidRequestFinishedInfoWrapper(
            String url,
            Collection<Object> annotations,
            Metrics metrics,
            int finishedReason,
            @Nullable UrlResponseInfo responseInfo,
            @Nullable CronetException exception) {
        super(url, annotations, metrics, finishedReason, responseInfo, exception);
    }

    @Override
    public Metrics getMetrics() {
        if (!sListenerSupportLimitedLogged) {
            Log.i(
                    TAG,
                    "RequestFinishedInfo.getMetrics() is unsupported when HttpEngineNativeProvider"
                            + " is used. The Metrics object will return null values.");
            sListenerSupportLimitedLogged = true;
        }
        return super.getMetrics();
    }

    static void reportFinished(
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations,
            RequestFinishedInfoListener listener,
            @RequestFinishedInfoImpl.FinishedReason int finishedReason,
            UrlResponseInfo responseInfo,
            CronetException exception) {
        RequestFinishedInfo requestInfo =
                new AndroidRequestFinishedInfoWrapper(
                        url,
                        annotations,
                        new CronetMetrics(
                                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, false, -1, -1),
                        finishedReason,
                        responseInfo,
                        exception);
        engine.reportRequestFinished(requestInfo, listener);
    }
}
