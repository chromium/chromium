// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.HttpException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;

import java.nio.ByteBuffer;
import java.util.Objects;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
@SuppressWarnings("Override")
class AndroidUrlRequestCallbackWrapper implements android.net.http.UrlRequest.Callback {
    private final org.chromium.net.UrlRequest.Callback mBackend;
    private AndroidUrlRequestWrapper mWrappedRequest;

    public AndroidUrlRequestCallbackWrapper(org.chromium.net.UrlRequest.Callback backend) {
        this.mBackend = Objects.requireNonNull(backend, "Callback is required.");
    }

    /**
     * @see <a
     *     href="https://developer.android.com/training/basics/network-ops/reading-network-state#listening-events">Foo
     *     Bar</a>
     */
    @Override
    public void onRedirectReceived(
            android.net.http.UrlRequest request,
            android.net.http.UrlResponseInfo info,
            String newLocationUrl)
            throws Exception {
        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onRedirectReceived(
                            mWrappedRequest, specializedResponseInfo, newLocationUrl);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onResponseStarted(
            android.net.http.UrlRequest request, android.net.http.UrlResponseInfo info)
            throws Exception {
        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onResponseStarted(mWrappedRequest, specializedResponseInfo);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onReadCompleted(
            android.net.http.UrlRequest request,
            android.net.http.UrlResponseInfo info,
            ByteBuffer byteBuffer)
            throws Exception {
        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onReadCompleted(mWrappedRequest, specializedResponseInfo, byteBuffer);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onSucceeded(
            android.net.http.UrlRequest request, android.net.http.UrlResponseInfo info) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        try {
            mBackend.onSucceeded(mWrappedRequest, specializedResponseInfo);
        } finally {
            // In a scenario where this throws, the side effect is that it will be propagated to
            // CronetUrlRequest as an error in the callback and mess with the FinalUserCallbackThrew
            // metrics. Because we catch most the exceptions, this side effect is negligible enough
            // to
            // not try to figure out a workaround.
            mWrappedRequest.maybeReportMetrics(
                    RequestFinishedInfo.SUCCEEDED, specializedResponseInfo, null);
        }
    }

    @Override
    public void onFailed(
            android.net.http.UrlRequest request,
            android.net.http.UrlResponseInfo info,
            HttpException error) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        CronetException translatedException =
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(error);
        try {
            mBackend.onFailed(mWrappedRequest, specializedResponseInfo, translatedException);
        } finally {
            // See comment in onSucceeded.
            mWrappedRequest.maybeReportMetrics(
                    RequestFinishedInfo.FAILED, specializedResponseInfo, translatedException);
        }
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.UrlRequest request,
            @Nullable android.net.http.UrlResponseInfo info) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        try {
            mBackend.onCanceled(mWrappedRequest, specializedResponseInfo);
        } finally {
            // See comment in onSucceeded.
            mWrappedRequest.maybeReportMetrics(
                    RequestFinishedInfo.CANCELED, specializedResponseInfo, null);
        }
    }

    void setRequest(AndroidUrlRequestWrapper request) {
        mWrappedRequest = request;
    }
}
