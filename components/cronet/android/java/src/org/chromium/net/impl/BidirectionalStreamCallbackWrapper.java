// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import java.nio.ByteBuffer;

@RequiresApi(api = 34)
@SuppressWarnings("Override")
class BidirectionalStreamCallbackWrapper implements android.net.http.BidirectionalStream.Callback {
    private final org.chromium.net.BidirectionalStream.Callback mBackend;

    public BidirectionalStreamCallbackWrapper(
            org.chromium.net.BidirectionalStream.Callback backend) {
        this.mBackend = backend;
    }

    @Override
    public void onStreamReady(android.net.http.BidirectionalStream bidirectionalStream) {
        AndroidBidirectionalStreamWrapper stream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onStreamReady(stream);
    }

    @Override
    public void onResponseHeadersReceived(android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onResponseHeadersReceived(specializedStream, specializedResponseInfo);
    }

    @Override
    public void onReadCompleted(android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo, ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onReadCompleted(
                specializedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onWriteCompleted(android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo, ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onWriteCompleted(
                specializedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onResponseTrailersReceived(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @NonNull android.net.http.UrlResponseInfo urlResponseInfo,
            @NonNull android.net.http.HeaderBlock headerBlock) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        AndroidHeaderBlockWrapper specializedHeaderBlock =
                new AndroidHeaderBlockWrapper(headerBlock);
        mBackend.onResponseTrailersReceived(
                specializedStream, specializedResponseInfo, specializedHeaderBlock);
    }

    @Override
    public void onSucceeded(android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onSucceeded(specializedStream, specializedResponseInfo);
    }

    @Override
    public void onFailed(android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo, HttpException e) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onFailed(specializedStream, specializedResponseInfo,
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(e));
    }

    @Override
    public void onCanceled(@NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @Nullable android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                new AndroidUrlResponseInfoWrapper(urlResponseInfo);
        AndroidBidirectionalStreamWrapper specializedStream =
                new AndroidBidirectionalStreamWrapper(bidirectionalStream);
        mBackend.onCanceled(specializedStream, specializedResponseInfo);
    }
}
