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
class AndroidUrlRequestCallbackWrapper implements android.net.http.UrlRequest.Callback {
    private final org.chromium.net.UrlRequest.Callback mBackend;
    private final Map<android.net.http.UrlRequest, AndroidUrlRequestWrapper>
            mHttpToWrappedRequestMap;

    public AndroidUrlRequestCallbackWrapper(org.chromium.net.UrlRequest.Callback backend) {
        Objects.requireNonNull(backend, "Callback is required.");
        this.mBackend = backend;
        mHttpToWrappedRequestMap = Collections.synchronizedMap(new HashMap<>());
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
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ false);

        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onRedirectReceived(
                            wrappedRequest, specializedResponseInfo, newLocationUrl);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onResponseStarted(
            android.net.http.UrlRequest request, android.net.http.UrlResponseInfo info)
            throws Exception {
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ false);

        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onResponseStarted(wrappedRequest, specializedResponseInfo);
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
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ false);

        CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> {
                    AndroidUrlResponseInfoWrapper specializedResponseInfo =
                            AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
                    mBackend.onReadCompleted(wrappedRequest, specializedResponseInfo, byteBuffer);
                    return null;
                },
                Exception.class);
    }

    @Override
    public void onSucceeded(
            android.net.http.UrlRequest request, android.net.http.UrlResponseInfo info) {
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        mBackend.onSucceeded(wrappedRequest, specializedResponseInfo);
    }

    @Override
    public void onFailed(
            android.net.http.UrlRequest request,
            android.net.http.UrlResponseInfo info,
            HttpException error) {
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        mBackend.onFailed(
                wrappedRequest,
                specializedResponseInfo,
                CronetExceptionTranslationUtils.translateCheckedAndroidCronetException(error));
    }

    @Override
    public void onCanceled(
            @NonNull android.net.http.UrlRequest request,
            @Nullable android.net.http.UrlResponseInfo info) {
        AndroidUrlRequestWrapper wrappedRequest =
                getWrappedStream(request, /* removeRecord= */ true);
        AndroidUrlResponseInfoWrapper specializedResponseInfo =
                AndroidUrlResponseInfoWrapper.createForUrlRequest(info);
        mBackend.onCanceled(wrappedRequest, specializedResponseInfo);
    }

    private AndroidUrlRequestWrapper getWrappedStream(
            @NonNull android.net.http.UrlRequest request, boolean removeRecord) {
        return Objects.requireNonNull(
                removeRecord
                        ? mHttpToWrappedRequestMap.remove(request)
                        : mHttpToWrappedRequestMap.get(request),
                "Expected android.net.http request to map to a wrapped request.");
    }

    /**
     * Records the mapping of {@link android.net.http.UrlRequest} to {@link
     * org.chromium.net.impl.AndroidUrlRequestWrapper}. This allows us to return the correct
     * wrappedRequest in the user callback instead of rewrapping the request in each method.
     *
     * <p>While our documentation does not specify that the request object in the callbacks is the
     * same object, it is an implicit expectation, as seen in the wild eg b/328442628, by our users
     * that we should not break.
     *
     * @param httpRequest the http request sent to the backend(ie HttpEngine implementation).
     * @param wrappedRequest The wrapped request object that was returned to user from
     *     requestBuilder.build()
     */
    void recordWrappedRequest(AndroidUrlRequestWrapper wrappedRequest) {
        if (mHttpToWrappedRequestMap.put(wrappedRequest.getBackend(), wrappedRequest) != null) {
            throw new IllegalStateException("WrappedRequest already recorded before.");
        }
    }

    @VisibleForTesting
    Map<android.net.http.UrlRequest, AndroidUrlRequestWrapper> getRequestRecordCopy() {
        return Collections.unmodifiableMap(mHttpToWrappedRequestMap);
    }
}
