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
class AndroidBidirectionalStreamCallbackWrapper
        implements android.net.http.BidirectionalStream.Callback {
    private final org.chromium.net.BidirectionalStream.Callback mBackend;
    private AndroidBidirectionalStreamWrapper mWrappedStream;

    public AndroidBidirectionalStreamCallbackWrapper(
            org.chromium.net.BidirectionalStream.Callback backend) {
        this.mBackend = Objects.requireNonNull(backend, "Callback is required.");
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
        try {
            mBackend.onSucceeded(mWrappedStream, specializedResponseInfo);
        } finally {
            // In a scenario where this throws, the side effect is that it will be propagated to
            // CronetUrlRequest as an error in the callback and mess with the FinalUserCallbackThrew
            // metrics. Because we catch most the exceptions, this side effect is negligible enough
            // to
            // not try to figure out a workaround.
            mWrappedStream.maybeReportMetrics(
                    RequestFinishedInfo.SUCCEEDED, specializedResponseInfo, null);
        }
    }

    @Override
    public void onFailed(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            HttpException e) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        CronetException exception =
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(e);
        try {
            mBackend.onFailed(mWrappedStream, specializedResponseInfo, exception);
        } finally {
            // See comment in onSucceeded.
            mWrappedStream.maybeReportMetrics(
                    RequestFinishedInfo.FAILED, specializedResponseInfo, exception);
        }
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @Nullable android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        try {
            mBackend.onCanceled(mWrappedStream, specializedResponseInfo);
        } finally {
            // See comment in onSucceeded.
            mWrappedStream.maybeReportMetrics(
                    RequestFinishedInfo.CANCELED, specializedResponseInfo, null);
        }
    }

    void setStream(AndroidBidirectionalStreamWrapper stream) {
        mWrappedStream = stream;
    }
}
