// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import androidx.annotation.Nullable;

import org.chromium.net.CallbackException;
import org.chromium.net.CronetException;
import org.chromium.net.UrlRequest;
import org.chromium.net.UrlResponseInfo;

import java.nio.ByteBuffer;

/**
 * An implementation of {@link UrlRequest.Callback} that takes away the difficulty of managing the
 * request lifecycle away, and automatically proceeds to read the response entirely.
 */
public abstract class ImplicitFlowControlCallback extends UrlRequest.Callback {
    private static final int BYTE_BUFFER_CAPACITY = 32 * 1024;

    /**
     * Invoked whenever a redirect is encountered. This will only be invoked between the call to
     * {@link UrlRequest#start} and {@link UrlRequest.Callback#onResponseStarted
     * onResponseStarted()}. The body of the redirect response, if it has one, will be ignored.
     *
     * @param info Response information.
     * @param newLocationUrl Location where request is redirected.
     * @return true if the redirect should be followed, false if the request should be canceled.
     * @throws Exception if an error occurs while processing a redirect. {@link #onFailed} will be
     * called with the thrown exception set as the cause of the {@link CallbackException}.
     */
    protected abstract boolean shouldFollowRedirect(UrlResponseInfo info, String newLocationUrl)
            throws Exception;

    /**
     * Invoked when the final set of headers, after all redirects, is received. Will only be invoked
     * once for each request. It's guaranteed that Cronet doesn't start reading the body until this
     * method returns.
     *
     * @param info Response information.
     * @throws Exception if an error occurs while processing response start. {@link #onFailed} will
     *         be
     * called with the thrown exception set as the cause of the {@link CallbackException}.
     */
    protected abstract void onResponseStarted(UrlResponseInfo info) throws Exception;

    /**
     * Invoked whenever part of the response body has been read. Only part of the buffer may be
     * populated, even if the entire response body has not yet been consumed. The buffer is ready
     * for reading. Buffers are reused internally so the implementing class shouldn't store the
     * buffer or use it anywhere else than in the implementation of this method.
     *
     * @param info Response information.
     * @param bodyChunk The buffer that contains the received data, flipped for reading.
     * @throws Exception if an error occurs while processing a read completion. {@link #onFailed}
     *         will
     * be called with the thrown exception set as the cause of the {@link CallbackException}.
     */
    protected abstract void onBodyChunkRead(UrlResponseInfo info, ByteBuffer bodyChunk)
            throws Exception;

    /**
     * Invoked when request is completed successfully. Once invoked, no other {@link
     * UrlRequest.Callback} methods will be invoked.
     *
     * @param info Response information.
     */
    protected abstract void onSucceeded(UrlResponseInfo info);

    /**
     * Invoked if request failed for any reason after {@link UrlRequest#start}. Once invoked, no
     * other
     * {@link UrlRequest.Callback} methods will be invoked. {@code error} provides information about
     * the failure.
     *
     * @param info Response information. May be {@code null} if no response was received.
     * @param exception information about error.
     */
    protected abstract void onFailed(@Nullable UrlResponseInfo info, CronetException exception);

    /**
     * Invoked if request was canceled via {@link UrlRequest#cancel}. Once invoked, no other {@link
     * UrlRequest.Callback} methods will be invoked.
     *
     * @param info Response information. May be {@code null} if no response was received.
     */
    protected abstract void onCanceled(@Nullable UrlResponseInfo info);

    @Override
    public final void onResponseStarted(UrlRequest request, UrlResponseInfo info) throws Exception {
        onResponseStarted(info);
        request.read(ByteBuffer.allocateDirect(BYTE_BUFFER_CAPACITY));
    }

    @Override
    public final void onRedirectReceived(
            UrlRequest request, UrlResponseInfo info, String newLocationUrl) throws Exception {
        if (shouldFollowRedirect(info, newLocationUrl)) {
            request.followRedirect();
        } else {
            request.cancel();
        }
    }

    @Override
    public final void onReadCompleted(
            UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) throws Exception {
        byteBuffer.flip();
        onBodyChunkRead(info, byteBuffer);
        byteBuffer.clear();
        request.read(byteBuffer);
    }

    @Override
    public final void onSucceeded(UrlRequest request, UrlResponseInfo info) {
        onSucceeded(info);
    }

    @Override
    public final void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
        onFailed(info, error);
    }

    @Override
    public final void onCanceled(UrlRequest request, UrlResponseInfo info) {
        onCanceled(info);
    }
}
