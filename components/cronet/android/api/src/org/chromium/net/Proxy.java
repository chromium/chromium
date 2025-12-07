// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
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
    @IntDef({HTTP, HTTPS, SCHEME_HTTP, SCHEME_HTTPS})
    public @interface Scheme {}

    /**
     * Establish a plaintext connection to the proxy itself.
     *
     * @deprecated Use {@link SCHEME_HTTP} instead.
     */
    @Deprecated public static final int HTTP = 0;

    /**
     * Establish a secure connection to the proxy itself.
     *
     * @deprecated Use {@link SCHEME_HTTP} instead.
     */
    @Deprecated public static final int HTTPS = 1;

    /** Establish a plaintext connection to the proxy itself. */
    public static final int SCHEME_HTTP = HTTP;

    /** Establish a secure connection to the proxy itself. */
    public static final int SCHEME_HTTPS = HTTPS;

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
        public void onBeforeRequest(@NonNull Request request) {
            onBeforeTunnelRequest(request);
        }

        /**
         * Called before sending a tunnel establishment request. Allows manipulating, or canceling,
         * said request before Cronet sends it to the proxy. Refer to {@link Request} to learn how a
         * request can be manipulated/canceled.
         *
         * @param request Represents the request that will be sent to the proxy.
         * @deprecated Override onBeforeRequest(Request) instead.
         */
        @Deprecated
        public void onBeforeTunnelRequest(@NonNull Request request) {
            try (request) {
                List<Map.Entry<String, String>> headers = onBeforeTunnelRequest();
                if (headers == null) {
                    return;
                }
                List<Pair<String, String>> convertedHeaders = new ArrayList<>();
                for (Map.Entry<String, String> header : headers) {
                    convertedHeaders.add(new Pair<>(header.getKey(), header.getValue()));
                }
                request.proceed(convertedHeaders);
            }
        }

        /**
         * Called before sending a tunnel establishment request. Allows adding headers that will be
         * sent only to the proxy as part of the tunnel establishment request. They will not be
         * added to the actual HTTP requests that will go through the proxy.
         *
         * <p>Warning: This will be called directly on Cronet's network thread, do not block. If
         * computing the headers is going to take a non-negligible amount of time, cancel and retry
         * the request once they are ready.
         *
         * <p>Returning headers which are not RFC 2616-compliant will cause a crash on the network
         * thread. TODO(https://crbug.com/425666408): Find a better way to surface this. It's
         * currently challenging since we're calling into the embedder code, not the other way
         * around. Making this API async (https://crbug.com/421341906) could be a way of solving
         * this, since we could throw IAE when the embedder calls back into Cronet.
         *
         * @return A list of headers to be added to the tunnel establishment request. This list can
         *     be empty, in which case no headers will be added. If {@code null} is returned, the
         *     tunnel connection will be canceled. When a tunnel connection is canceled, Cronet will
         *     interpret it as a failure to connect to this Proxy and will try the next Proxy in the
         *     list passed to {@link org.chromium.net.ProxyOptions} (refer to that class
         *     documentation for more info).
         * @deprecated Override onBeforeRequest(Request) instead.
         */
        @Deprecated
        public @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest() {
            throw new UnsupportedOperationException(
                    "At least one overload of onBeforeTunnelRequest must be overridden");
        }

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
        public @OnResponseReceivedAction int onResponseReceived(
                @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
            List<Map.Entry<String, String>> convertedResponseHeaders = new ArrayList<>();
            for (Pair<String, String> header : responseHeaders) {
                convertedResponseHeaders.add(
                        new AbstractMap.SimpleImmutableEntry<>(header.first, header.second));
            }
            return onTunnelHeadersReceived(convertedResponseHeaders, statusCode)
                    ? RESPONSE_ACTION_PROCEED
                    : RESPONSE_ACTION_CLOSE;
        }

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

        /**
         * Called after receiving a response to the tunnel establishment request. Allows reading
         * headers and status code of the response to the tunnel establishment request. This will
         * not be called for the actual HTTP requests that will go through the proxy.
         *
         * <p>Note: This is currently called for any response, whether it is a success or failure.
         * TODO(https://crbug.com/422429606): Do we really want this?
         *
         * @param responseHeaders The list of headers contained in the response to the tunnel
         *     establishment request.
         * @param statusCode The HTTP status code contained in the response to the tunnel
         *     establishment request.
         * @return {@code true} to allow using the tunnel connection to proxy requests. Allowing
         *     usage of a tunnel connection does not guarantee success, Cronet might still fail it
         *     aftewards (e.g., if the status code returned by the proxy is 407). {@code false} to
         *     cancel the tunnel connection. When a tunnel connection is canceled, Cronet will
         *     interpret it as a failure to connect to this Proxy and will try the next Proxy in the
         *     list passed to {@link org.chromium.net.ProxyOptions} (refer to that class
         *     documentation for more info).
         * @deprecated Override onResponseReceived instead.
         */
        @Deprecated
        public boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
            throw new UnsupportedOperationException(
                    "At least one of onResponseReceived or onTunnelHeadersReceived must be"
                            + " overriden");
        }
    }

    /**
     * Controls tunnel establishment requests. All methods will be invoked onto the Executor
     * specified in {@link Proxy}'s constructor.
     *
     * @deprecated Override HttpConnectCallback instead.
     */
    @Deprecated
    public abstract static class Callback extends HttpConnectCallback {}

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
            int port,
            @NonNull Executor executor,
            @NonNull HttpConnectCallback callback) {
        return new Proxy(scheme, host, port, executor, callback);
    }

    /**
     * Constructs a new proxy.
     *
     * @param scheme Type of proxy, as defined in {@link Scheme}.
     * @param host Hostname of the proxy.
     * @param port Port of the proxy.
     * @param executor Executor where {@link HttpConnectCallback} will be invoked.
     * @param callback Callback, as defined in {@link HttpConnectCallback}.
     * @deprecated Call {@link #createHttpProxy} instead.
     */
    @Deprecated
    public Proxy(
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
        this.mScheme = scheme;
        this.mHost = host;
        this.mPort = port;
        this.mExecutor = Objects.requireNonNull(executor);
        this.mCallback = Objects.requireNonNull(callback);
    }

    /**
     * Constructs a new proxy.
     *
     * @param scheme Type of proxy, as defined in {@link Scheme}.
     * @param host Hostname of the proxy.
     * @param port Port of the proxy.
     * @param callback Callback, as defined in {@link HttpConnectCallback}.
     * @deprecated Call {@link #createHttpProxy} instead.
     */
    @Deprecated
    public Proxy(
            @Scheme int scheme,
            @NonNull String host,
            int port,
            @NonNull HttpConnectCallback callback) {
        // Previously, we did not require an Executor and instead called Proxy.Callback directly
        // onto the network thread. Maintain backward compatibility by using an inline executor.
        this(
                /* scheme= */ scheme,
                /* host= */ host,
                /* port= */ port,
                /* executor= */ (Runnable r) -> {
                    r.run();
                },
                /* callback= */ callback);
    }

    /**
     * Returns the {@link Scheme} of this proxy.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public @Scheme int getScheme() {
        return mScheme;
    }

    /**
     * Returns the hostname of this proxy.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public @NonNull String getHost() {
        return mHost;
    }

    /**
     * Returns the port of this proxy.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public int getPort() {
        return mPort;
    }

    /**
     * Returns the {@link Executor} of this proxy.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public @NonNull Executor getExecutor() {
        return mExecutor;
    }

    /**
     * Returns the {@link HttpConnectCallback} of this proxy.
     *
     * @deprecated This will be made package private before Cronet proxy APIs are made
     *     non-experimental.
     */
    @Deprecated
    public @NonNull HttpConnectCallback getCallback() {
        return mCallback;
    }

    private final @Scheme int mScheme;
    private final @NonNull String mHost;
    private final int mPort;
    private final @NonNull Executor mExecutor;
    private final @NonNull HttpConnectCallback mCallback;
}
