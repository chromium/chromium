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
import androidx.annotation.VisibleForTesting;

import java.nio.ByteBuffer;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
@SuppressWarnings("Override")
class AndroidBidirectionalStreamCallbackWrapper
        implements android.net.http.BidirectionalStream.Callback {
    private final org.chromium.net.BidirectionalStream.Callback mBackend;
    private final Map<android.net.http.BidirectionalStream, AndroidBidirectionalStreamWrapper>
            mHttpToWrappedStreamMap;

    public AndroidBidirectionalStreamCallbackWrapper(
            org.chromium.net.BidirectionalStream.Callback backend) {
        Objects.requireNonNull(backend, "Callback is required.");
        this.mBackend = backend;
        mHttpToWrappedStreamMap = Collections.synchronizedMap(new HashMap<>());
    }

    @Override
    public void onStreamReady(android.net.http.BidirectionalStream bidirectionalStream) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ false);
        mBackend.onStreamReady(wrappedStream);
    }

    @Override
    public void onResponseHeadersReceived(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ false);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onResponseHeadersReceived(wrappedStream, specializedResponseInfo);
    }

    @Override
    public void onReadCompleted(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ false);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onReadCompleted(wrappedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onWriteCompleted(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            ByteBuffer byteBuffer,
            boolean endOfStream) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ false);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onWriteCompleted(wrappedStream, specializedResponseInfo, byteBuffer, endOfStream);
    }

    @Override
    public void onResponseTrailersReceived(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @NonNull android.net.http.UrlResponseInfo urlResponseInfo,
            @NonNull android.net.http.HeaderBlock headerBlock) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ false);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        AndroidHeaderBlockWrapper specializedHeaderBlock =
                new AndroidHeaderBlockWrapper(headerBlock);
        mBackend.onResponseTrailersReceived(
                wrappedStream, specializedResponseInfo, specializedHeaderBlock);
    }

    @Override
    public void onSucceeded(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onSucceeded(wrappedStream, specializedResponseInfo);
    }

    @Override
    public void onFailed(
            android.net.http.BidirectionalStream bidirectionalStream,
            android.net.http.UrlResponseInfo urlResponseInfo,
            HttpException e) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onFailed(
                wrappedStream,
                specializedResponseInfo,
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(e));
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            @Nullable android.net.http.UrlResponseInfo urlResponseInfo) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                getWrappedStream(bidirectionalStream, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForBidirectionalStream(urlResponseInfo);
        mBackend.onCanceled(wrappedStream, specializedResponseInfo);
    }

    private AndroidBidirectionalStreamWrapper getWrappedStream(
            @NonNull android.net.http.BidirectionalStream bidirectionalStream,
            boolean removeRecord) {
        return Objects.requireNonNull(
                removeRecord
                        ? mHttpToWrappedStreamMap.remove(bidirectionalStream)
                        : mHttpToWrappedStreamMap.get(bidirectionalStream),
                "Expected android.net.http.BidirectionalStream to map to an "
                        + "AndroidBidirectionalStreamWrapper stream.");
    }

    /**
     * Records the mapping of {@link android.net.http.BidirectionalStream} to {@link
     * org.chromium.net.impl.AndroidBidirectionalStreamWrapper}. This allows us to return the
     * correct wrappedStream to the user callback instead of rewrapping the stream in each method.
     *
     * <p>While our documentation does not specify that the stream object in the callbacks is the
     * same object, it is an implicit expectation, as seen in the wild eg b/328442628, by our users
     * that we should not break.
     *
     * @param wrappedStream The wrapped stream object that was returned to user from
     *     streamBuilder.build()
     */
    void recordWrappedStream(AndroidBidirectionalStreamWrapper wrappedStream) {
        if (mHttpToWrappedStreamMap.put(wrappedStream.getBackend(), wrappedStream) != null) {
            throw new IllegalStateException("WrappedStream already recorded before.");
        }
    }

    @VisibleForTesting
    Map<android.net.http.BidirectionalStream, AndroidBidirectionalStreamWrapper>
            getStreamRecordCopy() {
        return Collections.unmodifiableMap(mHttpToWrappedStreamMap);
    }
}
