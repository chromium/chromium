// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import androidx.annotation.Nullable;

import org.json.JSONObject;

import org.chromium.net.CronetException;
import org.chromium.net.UrlResponseInfo;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Future;

/**
 * Utility class for creating simple, convenient {@code UrlRequest.Callback} implementations for
 * reading common types of responses.
 *
 * <p>Note that the convenience callbacks store the entire response body in memory. We do not
 * recommend using them if it's possible to stream the response body, or if the response body sizes
 * can cause strain on the on-device resources.
 *
 * <p>The helper callbacks come in two flavors - either the caller provides a callback to be
 * invoked when the request finishes (successfully or not), or the caller is given a {@link Future}
 * which completes when Cronet finishes processing the request.
 */
public class UrlRequestCallbacks {
    public static ByteArrayCronetCallback forByteArrayBody(
            RedirectHandler redirectHandler, CronetRequestCompletionListener<byte[]> listener) {
        return newByteArrayCallback(redirectHandler).addCompletionListener(listener);
    }

    public static CallbackAndResponseFuturePair<byte[], ByteArrayCronetCallback> forByteArrayBody(
            RedirectHandler redirectHandler) {
        ByteArrayCronetCallback callback = newByteArrayCallback(redirectHandler);
        Future<CronetResponse<byte[]>> future = addResponseFutureListener(callback);
        return new CallbackAndResponseFuturePair<>(future, callback);
    }

    public static StringCronetCallback forStringBody(
            RedirectHandler redirectHandler, CronetRequestCompletionListener<String> listener) {
        return newStringCallback(redirectHandler).addCompletionListener(listener);
    }

    public static CallbackAndResponseFuturePair<String, StringCronetCallback> forStringBody(
            RedirectHandler redirectHandler) {
        StringCronetCallback callback = newStringCallback(redirectHandler);
        Future<CronetResponse<String>> future = addResponseFutureListener(callback);
        return new CallbackAndResponseFuturePair<>(future, callback);
    }

    public static JsonCronetCallback forJsonBody(
            RedirectHandler redirectHandler, CronetRequestCompletionListener<JSONObject> listener) {
        return newJsonCallback(redirectHandler).addCompletionListener(listener);
    }

    public static CallbackAndResponseFuturePair<JSONObject, JsonCronetCallback> forJsonBody(
            RedirectHandler redirectHandler) {
        JsonCronetCallback callback = newJsonCallback(redirectHandler);
        Future<CronetResponse<JSONObject>> future = addResponseFutureListener(callback);
        return new CallbackAndResponseFuturePair<>(future, callback);
    }

    private static ByteArrayCronetCallback newByteArrayCallback(RedirectHandler redirectHandler) {
        return new ByteArrayCronetCallback() {
            @Override
            protected boolean shouldFollowRedirect(UrlResponseInfo info, String newLocationUrl)
                    throws Exception {
                return redirectHandler.shouldFollowRedirect(info, newLocationUrl);
            }
        };
    }

    private static StringCronetCallback newStringCallback(RedirectHandler redirectHandler) {
        return new StringCronetCallback() {
            @Override
            protected boolean shouldFollowRedirect(UrlResponseInfo info, String newLocationUrl)
                    throws Exception {
                return redirectHandler.shouldFollowRedirect(info, newLocationUrl);
            }
        };
    }

    private static JsonCronetCallback newJsonCallback(RedirectHandler redirectHandler) {
        return new JsonCronetCallback() {
            @Override
            protected boolean shouldFollowRedirect(UrlResponseInfo info, String newLocationUrl)
                    throws Exception {
                return redirectHandler.shouldFollowRedirect(info, newLocationUrl);
            }
        };
    }

    private static <T> Future<CronetResponse<T>> addResponseFutureListener(
            InMemoryTransformCronetCallback<T> callback) {
        CompletableFuture<CronetResponse<T>> completableFuture = new CompletableFuture<>();
        callback.addCompletionListener(
                new CronetRequestCompletionListener<T>() {
                    @Override
                    public void onFailed(
                            @Nullable UrlResponseInfo info, CronetException exception) {
                        completableFuture.completeExceptionally(exception);
                    }

                    @Override
                    public void onCanceled(@Nullable UrlResponseInfo info) {
                        completableFuture.completeExceptionally(
                                new CronetException("The request was canceled!", null) {});
                    }

                    @Override
                    public void onSucceeded(UrlResponseInfo info, T body) {
                        completableFuture.complete(new CronetResponse<>(info, body));
                    }
                });
        return completableFuture;
    }

    /**
     * A named pair-like structure encapsulating Cronet callbacks and associated response futures.
     *
     * <p>The request should be used to pass to {@code CronetEngine.newUrlRequest()}, the future
     * will contain the response to the request.
     *
     * @param <CallbackT> the subtype of the callback
     * @param <ResponseBodyT> The type of the deserialized response body
     */
    public static class CallbackAndResponseFuturePair<
            ResponseBodyT, CallbackT extends InMemoryTransformCronetCallback<ResponseBodyT>> {
        private final Future<CronetResponse<ResponseBodyT>> mFuture;
        private final CallbackT mCallback;

        CallbackAndResponseFuturePair(
                Future<CronetResponse<ResponseBodyT>> future, CallbackT callback) {
            this.mFuture = future;
            this.mCallback = callback;
        }

        public Future<CronetResponse<ResponseBodyT>> getFuture() {
            return mFuture;
        }

        public CallbackT getCallback() {
            return mCallback;
        }
    }

    private UrlRequestCallbacks() {}
}
