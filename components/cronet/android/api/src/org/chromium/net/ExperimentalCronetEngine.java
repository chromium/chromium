// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.util.Date;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * {@link CronetEngine} that exposes experimental features. To obtain an instance of this class,
 * cast a {@code CronetEngine} to this type. Every instance of {@code CronetEngine} can be cast to
 * an instance of this class, as they are backed by the same implementation and hence perform
 * identically. Instances of this class are not meant for general use, but instead only to access
 * experimental features. Experimental features may be deprecated in the future. Use at your own
 * risk.
 *
 * <p>{@hide since this class exposes experimental features that should be hidden.}
 *
 * @deprecated scheduled for deletion, don't use in new code.
 */
@Deprecated
public abstract class ExperimentalCronetEngine extends CronetEngine {
    /** The value of a connection metric is unknown. */
    public static final int CONNECTION_METRIC_UNKNOWN = CronetEngine.CONNECTION_METRIC_UNKNOWN;

    /**
     * The estimate of the effective connection type is unknown.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_UNKNOWN =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

    /**
     * The device is offline.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_OFFLINE =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_OFFLINE;

    /**
     * The estimate of the effective connection type is slow 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_SLOW_2G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_SLOW_2G;

    /**
     * The estimate of the effective connection type is 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_2G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_2G;

    /**
     * The estimate of the effective connection type is 3G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_3G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_3G;

    /**
     * The estimate of the effective connection type is 4G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_4G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_4G;

    /** The value to be used to undo any previous network binding. */
    public static final long UNBIND_NETWORK_HANDLE = CronetEngine.UNBIND_NETWORK_HANDLE;

    /**
     * A version of {@link CronetEngine.Builder} that exposes experimental features. Instances of
     * this class are not meant for general use, but instead only to access experimental features.
     * Experimental features may be deprecated in the future. Use at your own risk.
     */
    public static class Builder extends CronetEngine.Builder {
        /**
         * Constructs a {@link Builder} object that facilitates creating a {@link CronetEngine}. The
         * default configuration enables HTTP/2 and disables QUIC, SDCH and the HTTP cache.
         *
         * @param context Android {@link Context}, which is used by the Builder to retrieve the
         *     application context. A reference to only the application context will be kept, so as
         *     to avoid extending the lifetime of {@code context} unnecessarily.
         */
        public Builder(Context context) {
            super(context);
        }

        /**
         * Constructs {@link Builder} with a given delegate that provides the actual implementation
         * of the {@code Builder} methods. This constructor is used only by the internal
         * implementation.
         *
         * @param builderDelegate delegate that provides the actual implementation.
         *     <p>{@hide}
         */
        public Builder(ICronetEngineBuilder builderDelegate) {
            super(builderDelegate);
        }

        /**
         * Sets experimental options to be used in Cronet.
         *
         * @param options JSON formatted experimental options.
         * @return the builder to facilitate chaining.
         */
        public Builder setExperimentalOptions(String options) {
            mParsedExperimentalOptions =
                    ExperimentalOptionsTranslator.toJsonExperimentalOptions(options);
            return this;
        }

        /**
         * Returns delegate, only for testing.
         *
         * @hide
         */
        @VisibleForTesting
        public ICronetEngineBuilder getBuilderDelegate() {
            return mBuilderDelegate;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public Builder setUserAgent(String userAgent) {
            super.setUserAgent(userAgent);
            return this;
        }

        @Override
        public Builder setStoragePath(String value) {
            super.setStoragePath(value);
            return this;
        }

        @Override
        public Builder setLibraryLoader(LibraryLoader loader) {
            super.setLibraryLoader(loader);
            return this;
        }

        @Override
        public Builder enableQuic(boolean value) {
            super.enableQuic(value);
            return this;
        }

        @Override
        public Builder enableHttp2(boolean value) {
            super.enableHttp2(value);
            return this;
        }

        @Override
        @QuicOptions.Experimental
        public Builder setQuicOptions(QuicOptions options) {
            super.setQuicOptions(options);
            return this;
        }

        @Override
        @DnsOptions.Experimental
        public Builder setDnsOptions(DnsOptions options) {
            super.setDnsOptions(options);
            return this;
        }

        @Override
        @ConnectionMigrationOptions.Experimental
        public Builder setConnectionMigrationOptions(ConnectionMigrationOptions options) {
            super.setConnectionMigrationOptions(options);
            return this;
        }

        @Override
        public Builder enableSdch(boolean value) {
            return this;
        }

        @Override
        public Builder enableHttpCache(int cacheMode, long maxSize) {
            super.enableHttpCache(cacheMode, maxSize);
            return this;
        }

        @Override
        public Builder addQuicHint(String host, int port, int alternatePort) {
            super.addQuicHint(host, port, alternatePort);
            return this;
        }

        @Override
        public Builder addPublicKeyPins(
                String hostName,
                Set<byte[]> pinsSha256,
                boolean includeSubdomains,
                Date expirationDate) {
            super.addPublicKeyPins(hostName, pinsSha256, includeSubdomains, expirationDate);
            return this;
        }

        @Override
        public Builder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
            super.enablePublicKeyPinningBypassForLocalTrustAnchors(value);
            return this;
        }

        @Override
        public Builder enableNetworkQualityEstimator(boolean value) {
            super.enableNetworkQualityEstimator(value);
            return this;
        }

        @Override
        public Builder setThreadPriority(int priority) {
            super.setThreadPriority(priority);
            return this;
        }

        @Override
        public ExperimentalCronetEngine build() {
            return buildExperimental();
        }
    }

    @Override
    public abstract ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor);

    @Override
    public abstract ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, UrlRequest.Callback callback, Executor executor);

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url} using
     * the given proxy. <p> <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation
     * is subject to certain limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @param proxy proxy to use when establishing connection.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     * @throws IOException if an error occurs while opening the connection.
     */
    // TODO(pauljensen): Expose once implemented, http://crbug.com/418111
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        return url.openConnection(proxy);
    }
}
