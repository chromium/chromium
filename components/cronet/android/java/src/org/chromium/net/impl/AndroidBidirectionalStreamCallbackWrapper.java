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
class AndroidBidirectionalStreamCallbackWrapper
        implements android.net.http.BidirectionalStream.Callback {
    private final org.chromium.net.BidirectionalStream.Callback mBackend;
    private AndroidBidirectionalStreamWrapper mWrappedStream;

    public AndroidBidirectionalStreamCallbackWrapper(
            org.chromium.net.BidirectionalStream.Callback backend) {
        Objects.requireNonNull(backend, "Callback is required.");
        this.mBackend = backend;
    }

    @Override
    public void onStreamReady(android.net.http.BidirectionalStream bidirectionalStream) {
        mBackend.onStreamReady(mWrappedStream);
    }

    @Override
    public void onResponseHeadersReceived(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onResponseHeadersReceived(mWrappedStream, specializedResponseInfo);
    }

    @Override
    public void onReadCompleted(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onReadCompleted(mWrappedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onWriteCompleted(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onWriteCompleted(mWrappedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onResponseTrailersReceived(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @NonNull android.net.http.UrlResponseInfo urlResponseInfo,
            @NonNull android.net.http.HeaderBlock headerBlock) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        AndroidHeaderBlockWrapper specializedHeaderBlock =
                new AndroidHeaderBlockWrapper(headerBlock);
        mBackend.onResponseTrailersReceived(
                mWrappedStream, specializedResponseInfo, specializedHeaderBlock);
    }

    @Override
    public void onSucceeded(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onSucceeded(mWrappedStream, specializedResponseInfo);
    }

    @Override
    public void onFailed(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            HttpException e) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onFailed(
                mWrappedStream,
                specializedResponseInfo,
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(e));
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @Nullable android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onCanceled(mWrappedStream, specializedResponseInfo);
    }

    void setStream(AndroidBidirectionalStreamWrapper stream) {
        mWrappedStream = stream;
    }
}
