// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Represents a proxy that can be used by Cronet. Throughout this file we say "tunnel establishment
 * request". This is intentionally vague: establishing connections via a proxy is a complicated
 * subject, how it is established depends on many implementation details (e.g., it could end up
 * being a simple GET, a GET with connection upgrade, a CONNECT or an extended CONNECT).
 */
public final class Proxy {

    /**
     * Types of proxies supported by {@link Proxy}. Specifies how Cronet establishes a connection to
     * the proxy. Note this only affects how Cronet connects to the proxy, not how it connects to
     * the final destination. Cronet will always negotiate an end-to-end secure connection to the
     * destination if the destination uses HTTPS, regardless of the scheme used by the proxy.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({HTTP, HTTPS})
    public @interface Scheme {}

    /** Establish a plaintext connection to the proxy itself. */
    public static final int HTTP = 0;

    /** Establish a secure connection to the proxy itself. */
    public static final int HTTPS = 1;

    /** Controls tunnel establishment requests. */
    public abstract static class Callback {
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
         */
        public abstract @Nullable List<Map.Entry<String, String>> onBeforeTunnelRequest();

        /**
         * Called after receiving a response to the tunnel establishment request. Allows reading
         * headers and status code of the response to the tunnel establishment request. This will
         * not be called for the actual HTTP requests that will go through the proxy.
         *
         * <p>Note: This is currently called for any response, whether it is a success or failure.
         * TODO(https://crbug.com/422429606): Do we really want this?
         *
         * <p>Warning: This will be called directly on Cronet's network thread, do not block.
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
         */
        public abstract boolean onTunnelHeadersReceived(
                @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode);
    }

    /**
     * Constructs a new proxy.
     *
     * @param scheme Type of proxy, as defined in {@link Scheme}.
     * @param host Hostname of the proxy.
     * @param port Port of the proxy.
     * @param callback Callback, as defined in {@link Callback}, that gets invoked on different
     *     events.
     */
    public Proxy(@Scheme int scheme, @NonNull String host, int port, @NonNull Callback callback) {
        if (scheme != HTTP && scheme != HTTPS) {
            throw new IllegalArgumentException(String.format("Unknown scheme %s", scheme));
        }
        this.mScheme = scheme;
        this.mHost = Objects.requireNonNull(host);
        this.mPort = port;
        this.mCallback = Objects.requireNonNull(callback);
    }

    /** Returns the {@link Scheme} of this proxy. */
    public @Scheme int getScheme() {
        return mScheme;
    }

    /** Returns the hostname of this proxy. */
    public @NonNull String getHost() {
        return mHost;
    }

    /** Returns the port of this proxy. */
    public int getPort() {
        return mPort;
    }

    /** Returns the {@link Callback} of this proxy. */
    public @NonNull Callback getCallback() {
        return mCallback;
    }

    private final @Scheme int mScheme;
    private final @NonNull String mHost;
    private final int mPort;
    private final @NonNull Callback mCallback;
}
