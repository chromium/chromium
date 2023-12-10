// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import androidx.annotation.Nullable;

import org.chromium.net.CronetException;
import org.chromium.net.UrlResponseInfo;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.WritableByteChannel;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * An abstract Cronet callback that reads the entire body to memory and optionally deserializes the
 * body before passing it back to the issuer of the HTTP request.
 *
 * <p>The requester can subscribe for updates about the request by adding completion mListeners on
 * the callback. When the request reaches a terminal state, the mListeners are informed in order of
 * addition.
 *
 * @param <T> the response body type
 */
public abstract class InMemoryTransformCronetCallback<T> extends ImplicitFlowControlCallback {
    private static final String CONTENT_LENGTH_HEADER_NAME = "Content-Length";
    // See ArrayList.MAX_ARRAY_SIZE for reasoning.
    private static final int MAX_ARRAY_SIZE = Integer.MAX_VALUE - 8;

    private ByteArrayOutputStream mResponseBodyStream;
    private WritableByteChannel mResponseBodyChannel;

    /** The set of listeners observing the associated request. */
    private final Set<CronetRequestCompletionListener<? super T>> mListeners =
            new LinkedHashSet<>();

    /**
     * Transforms (deserializes) the plain full body into a user-defined object.
     *
     * <p>It is assumed that the implementing classes handle edge cases (such as empty and malformed
     * bodies) appropriately. Cronet doesn't inspects the objects and passes them (or any
     * exceptions) along to the issuer of the request.
     */
    protected abstract T transformBodyBytes(UrlResponseInfo info, byte[] bodyBytes);

    /**
     * Adds a completion listener. All listeners are informed when the request reaches a terminal
     * state, in order of addition. If a listener is added multiple times, it will only be called
     * once according to the first time it was added.
     *
     * @see CronetRequestCompletionListener
     */
    public ImplicitFlowControlCallback addCompletionListener(
            CronetRequestCompletionListener<? super T> listener) {
        mListeners.add(listener);
        return this;
    }

    @Override
    protected final void onResponseStarted(UrlResponseInfo info) {
        long bodyLength = getBodyLength(info);
        if (bodyLength > MAX_ARRAY_SIZE) {
            throw new IllegalArgumentException(
                    "The body is too large and wouldn't fit in a byte array!");
        }
        // bodyLength returns -1 if the header can't be parsed, also ignore obviously bogus values
        if (bodyLength >= 0) {
            mResponseBodyStream = new ByteArrayOutputStream((int) bodyLength);
        } else {
            mResponseBodyStream = new ByteArrayOutputStream();
        }
        mResponseBodyChannel = Channels.newChannel(mResponseBodyStream);
    }

    @Override
    protected final void onBodyChunkRead(UrlResponseInfo info, ByteBuffer bodyChunk)
            throws Exception {
        mResponseBodyChannel.write(bodyChunk);
    }

    @Override
    protected final void onSucceeded(UrlResponseInfo info) {
        T body = transformBodyBytes(info, mResponseBodyStream.toByteArray());
        for (CronetRequestCompletionListener<? super T> callback : mListeners) {
            callback.onSucceeded(info, body);
        }
    }

    @Override
    protected final void onFailed(@Nullable UrlResponseInfo info, CronetException exception) {
        for (CronetRequestCompletionListener<? super T> callback : mListeners) {
            callback.onFailed(info, exception);
        }
    }

    @Override
    protected final void onCanceled(@Nullable UrlResponseInfo info) {
        for (CronetRequestCompletionListener<? super T> callback : mListeners) {
            callback.onCanceled(info);
        }
    }

    /** Returns the numerical value of the Content-Length header, or -1 if not set or invalid. */
    private static long getBodyLength(UrlResponseInfo info) {
        List<String> contentLengthHeader = info.getAllHeaders().get(CONTENT_LENGTH_HEADER_NAME);
        if (contentLengthHeader == null || contentLengthHeader.size() != 1) {
            return -1;
        }
        try {
            return Long.parseLong(contentLengthHeader.get(0));
        } catch (NumberFormatException e) {
            return -1;
        }
    }
}
