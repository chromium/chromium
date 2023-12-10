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
import java.nio.ByteBuffer;
import java.util.Objects;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
@SuppressWarnings("Override")
class AndroidUrlRequestCallbackWrapper implements android.net.http.UrlRequest.Callback {
    private final org.chromium.net.UrlRequest.Callback mBackend;

    public AndroidUrlRequestCallbackWrapper(org.chromium.net.UrlRequest.Callback backend) {
        Objects.requireNonNull(backend, "Callback is required.");
        this.mBackend = backend;
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
                    AndroidUrlRequestWrapper specializedRequest =
                            new AndroidUrlRequestWrapper(request);
                    mBackend.onRedirectReceived(
                            specializedRequest, specializedResponseInfo, newLocationUrl);
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
                    AndroidUrlRequestWrapper specializedRequest =
                            new AndroidUrlRequestWrapper(request);
                    mBackend.onResponseStarted(specializedRequest, specializedResponseInfo);
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
                    AndroidUrlRequestWrapper specializedRequest =
                            new AndroidUrlRequestWrapper(request);
                    mBackend.onReadCompleted(
                            specializedRequest, specializedResponseInfo, byteBuffer);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onSucceeded(
            android.net.http.UrlRequest request, android.net.http.UrlResponseInfo info) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        AndroidUrlRequestWrapper specializedRequest = new AndroidUrlRequestWrapper(request);
        mBackend.onSucceeded(specializedRequest, specializedResponseInfo);
    }

    @Override
    public void onFailed(
            android.net.http.UrlRequest request,
            android.net.http.UrlResponseInfo info,
            HttpException error) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        AndroidUrlRequestWrapper specializedRequest = new AndroidUrlRequestWrapper(request);
        mBackend.onFailed(
                specializedRequest,
                specializedResponseInfo,
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(error));
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.UrlRequest request,
            @Nullable android.net.http.UrlResponseInfo info) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        AndroidUrlRequestWrapper specializedRequest = new AndroidUrlRequestWrapper(request);
        mBackend.onCanceled(specializedRequest, specializedResponseInfo);
    }
}
