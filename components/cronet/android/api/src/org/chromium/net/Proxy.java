// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.IntRange;
import androidx.annotation.NonNull;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executor;

/**
 * Represents a proxy that can be used by {@link CronetEngine}.
 *
 * <p>How HTTP requests are sent via proxies depends on a variety of factors. For example: the type
 * of proxy being used, the HTTP version being used, whether the request is being sent to a
 * destination with an http or https URI scheme. Additionally, whether a tunnel through the proxy is
 * established, or not, also depends on these factors.
 */
public final class Proxy {

    /**
     * Schemes supported when defining a proxy.
     *
     * <p>This only affects how CronetEngine interacts with proxies, not how it connects to
     * destinations of HTTP requests. CronetEngine always negotiates an end-to-end secure connection
     * for destinations with an https URI scheme, regardless of the scheme used to identify proxies.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SCHEME_HTTP, SCHEME_HTTPS})
    public @interface Scheme {}

    /** Establish a plaintext connection to the proxy itself. */
    public static final int SCHEME_HTTP = 0;

    /** Establish a secure connection to the proxy itself. */
    public static final int SCHEME_HTTPS = 1;

    /**
     * Controls tunnels established via HTTP CONNECT. All methods will be invoked onto the Executor
     * specified in {@link #createHttpProxy}.
     *
     * <p>Methods within this class are invoked only when both of the following are true:
     *
     * <pre>
     * - HttpEngine must establish a connection to the destination of an HTTP request
     * - The destination of the HTTP request has an https URI scheme
     * </pre>
     *
     * Refer to {@link #createHttpProxy}'s documentation to understand how CronetEngine decides
     * whether to establish a tunnel through an HTTP proxy.
     */
    public abstract static class HttpConnectCallback {
        /**
         * Represents an HTTP CONNECT request being sent to the proxy server.
         *
         * <p>All methods may be called synchronously or asynchronously, on any thread.
         */
        public abstract static class Request implements AutoCloseable {
            /**
             * Allows the tunnel establishment request to proceed, adding the extra headers to the
             * underlying HTTP CONNECT request.
             *
             * @param extraHeaders A list of RFC 2616-compliant headers to be added to HTTP CONNECT
             *     request directed to the proxy server. This list can be empty, in which case no
             *     headers will be added. Note: these headers won't be added to the HTTP requests
             *     that will go through the tunnel once it is established.
             * @throws IllegalArgumentException If any of the headers is not RFC 2616-compliant.
             * @throws IllegalStateException If this method is called multiple times, or after
             *     {@link #close} has been called.
             */
            public abstract void proceed(@NonNull List<Pair<String, String>> extraHeaders);

            /**
             * Releases all resources associated with the tunnel establishment request.
             *
             * <p>If this method is called after {@link #proceed}, this will have no effect on the
             * tunnel establishment request and the underlying HTTP CONNECT, they will keep
             * proceeding.
             *
             * <p>If this method is called before {@link #proceed}, the tunnel establishment request
             * and the underlying HTTP CONNECT request will be canceled.
             *
             * <p>When a tunnel establishment request is canceled, CronetEngine will interpret it as
             * a failure to use the associated {@link Proxy}. CronetEngine will then try the next
             * {@link Proxy} in the list passed to {@link ProxyOptions} (refer to that class
             * documentation for more info).
             */
            @Override
            public abstract void close();
        }

        /**
         * Called before sending an HTTP CONNECT request to the proxy to establish a tunnel.
         *
         * <p>Allows manipulating, or canceling, said request before sending it to the proxy. Refer
         * to {@link Request} to learn how a request can be manipulated/canceled.
         *
         * @param request Represents the HTTP CONNECT request that will be sent to the proxy.
         */
        public abstract void onBeforeRequest(@NonNull Request request);

        /**
         * Called after receiving a response to the HTTP CONNECT request sent to the proxy to
         * establish a tunnel. Allows reading headers and status code.
         *
         * <p>When sending HTTP requests to a destination with an https URI schene, CronetEngine
         * will never use a proxy whose {@link #onResponseReceived} callback has not been invoked.
         *
         * <p>This will not be called for HTTP requests that will go through the tunnel once it is
         * established.
         *
         * <p>If this method throws any {@link java.lang.Throwable}, the fate of the tunnel will be
         * as if {@link RESPONSE_ACTION_CLOSE} had been returned (the {@link java.lang.Throwable}
         * will not be caught).
         *
         * @param responseHeaders The list of headers contained in the response to the HTTP CONNECT
         *     request.
         * @param statusCode The HTTP status code contained in the response to the HTTP CONNECT
         *     request.
         * @return A {@link OnResponseReceivedAction} value representing what should be done with
         *     this tunnel connection. Refer to {@link OnResponseReceivedAction} documentation.
         */
        public abstract @OnResponseReceivedAction int onResponseReceived(
                @NonNull List<Pair<String, String>> responseHeaders, int statusCode);

        /**
         * Actions that can be performed in response to {@link #onResponseReceived} being called.
         */
        @Retention(RetentionPolicy.SOURCE)
        @IntDef({RESPONSE_ACTION_CLOSE, RESPONSE_ACTION_PROCEED})
        public @interface OnResponseReceivedAction {}

        /**
         * Closes the tunnel connection, preventing it from being used to send HTTP requests.
         *
         * <p>When a tunnel connection is closed, CronetEngine will interpret it as a failure to use
         * the associated {@link Proxy}. CronetEngine will then try the next {@link Proxy} in the
         * list passed to {@link ProxyOptions} (refer to that class documentation for more info).
         */
        public static final int RESPONSE_ACTION_CLOSE = 0;

        /**
         * Proceeds establishing a tunnel.
         *
         * <p>This does not guarantee that the tunnel will successfully be established and used to
         * send HTTP requests: CronetEngine will perform additional checks prior to that. Depending
         * on their outcome, CronetEngine might still decide to close the tunnel connection. If the
         * tunnel connection ends up being closed by CronetEngine, it will be considered as a
         * failure to use the associated {@link Proxy}. CronetEngine will then try the next {@link
         * Proxy} in the list passed to {@link ProxyOptions} (refer to that class documentation for
         * more info)
         */
        public static final int RESPONSE_ACTION_PROCEED = 1;
    }

    /**
     * Constructs an HTTP proxy.
     *
     * <p>When sending HTTP requests via an HTTP proxy, whether {@code callback} is called, or not,
     * depends on the URI scheme of the destination:
     *
     * <ul>
     *   <li>For destinations with an https URI scheme, CronetEngine establishes a tunnel through
     *       the proxy. The tunnel is established via an HTTP CONNECT request. In this case {@code
     *       callback} will be called to control the HTTP CONNECT request used to establish the
     *       tunnel.
     *   <li>For destinations with an http URI scheme, CronetEngine sends an HTTP request,
     *       containing the entire URI of the destination, to the proxy. In this case {@code
     *       callback} will not be called.
     * </ul>
     *
     * @param scheme {@link Scheme} that, alongside {@code host} and {@code port}, identifies this
     *     proxy.
     * @param host Non-empty host that, alongside {@code scheme} and {@code port}, identifies this
     *     proxy.
     * @param port Port that, alongside {@code scheme} and {@code host}, identifies this proxy. Its
     *     value must be within [0, 65535].
     * @param executor Executor where {@code callback} will be invoked.
     * @param callback Callback that allows interacting with the HTTP CONNECT request, and its
     *     response, that CronetEngine sends to establish tunnels through the proxy.
     */
    public static @NonNull Proxy createHttpProxy(
            @Scheme int scheme,
            @NonNull String host,
            @IntRange(from = 0, to = 65535) int port,
            @NonNull Executor executor,
            @NonNull HttpConnectCallback callback) {
        return new Proxy(scheme, host, port, executor, callback);
    }

    @Scheme
    int getScheme() {
        return mScheme;
    }

    @NonNull
    String getHost() {
        return mHost;
    }

    int getPort() {
        return mPort;
    }

    @NonNull
    Executor getExecutor() {
        return mExecutor;
    }

    @NonNull
    HttpConnectCallback getCallback() {
        return mCallback;
    }

    private Proxy(
            @Scheme int scheme,
            @NonNull String host,
            int port,
            @NonNull Executor executor,
            @NonNull HttpConnectCallback callback) {
        if (scheme != SCHEME_HTTP && scheme != SCHEME_HTTPS) {
            throw new IllegalArgumentException(String.format("Unknown scheme %s", scheme));
        }
        if (host.equals("")) {
            throw new IllegalArgumentException("host cannot be an empty string");
        }
        if (port < 0 || port > 65535) {
            throw new IllegalArgumentException(
                    String.format("port must be within [0, 65535] but it was: %d", port));
        }
        mScheme = scheme;
        mHost = host;
        mPort = port;
        mExecutor = Objects.requireNonNull(executor);
        mCallback = Objects.requireNonNull(callback);
    }

    private final @Scheme int mScheme;
    private final @NonNull String mHost;
    private final int mPort;
    private final @NonNull Executor mExecutor;
    private final @NonNull HttpConnectCallback mCallback;
}
