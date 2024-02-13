// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.os.Build.VERSION_CODES;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresOptIn;

import java.time.Duration;
import java.util.Objects;

/**
 * A class configuring Cronet's host resolution functionality. Note that while we refer to {@code
 * DNS} as the most common mechanism being used for brevity, settings apply to other means of
 * resolving hostnames like hosts file resolution.
 *
 * <p>Cronet resolve hostnames in two ways - either by using the system resolver (using {@code
 * getaddrinfo} provided by system libraries), or by using a custom resolver which is built into the
 * networking stack Cronet uses.
 *
 * <p>The built-in stack provides several advantages over using the system resolver:
 *
 * <ul>
 *   <li>It has been tailored to the needs of the networking stack, particularly speed and
 *       stability.
 *   <li>{@code getaddrinfo} is a blocking call which requires dedicating worker threads and makes
 *       cancellation impossible (we need to abandon the thread until the call completes)
 *   <li>The {@code getaddrinfo} interface gives no insight into the root cause of failures
 *   <li>{@code struct addrinfo} provides no TTL (Time To Live) of the returned addresses. This
 *       restricts flexibility of handling caching (e.g. allowing the use of stale DNS records) and
 *       requires us to either rely on OS DNS caches, or be extremely conservative with the TTL.
 *   <li>As part of the OS, {@code getaddrinfo} evolves slowly. Using a custom stack enables Cronet
 *       to introduce features like encrypted DNS faster.
 * </ul>
 *
 * <p>Most configuration in this class is only applicable if the built-in DNS resolver is used.
 */
public final class DnsOptions {
    @Nullable private final Boolean mUseBuiltInDnsResolver;
    @Nullable private final Boolean mPersistHostCache;
    @Nullable private final Boolean mEnableStaleDns;
    @Nullable private final Long mPersistHostCachePeriodMillis;

    @Nullable private final Boolean mPreestablishConnectionsToStaleDnsResults;
    @Nullable private final StaleDnsOptions mStaleDnsOptions;

    DnsOptions(Builder builder) {
        this.mEnableStaleDns = builder.mEnableStaleDns;
        this.mStaleDnsOptions = builder.mStaleDnsOptions;
        this.mPersistHostCachePeriodMillis = builder.mPersistHostCachePeriodMillis;
        this.mPreestablishConnectionsToStaleDnsResults =
                builder.mPreestablishConnectionsToStaleDnsResults;
        this.mUseBuiltInDnsResolver = builder.mUseBuiltInDnsResolver;
        this.mPersistHostCache = builder.mPersistHostCache;
    }

    @Nullable
    public Boolean getUseBuiltInDnsResolver() {
        return mUseBuiltInDnsResolver;
    }

    @Nullable
    public Boolean getPersistHostCache() {
        return mPersistHostCache;
    }

    @Nullable
    public Boolean getEnableStaleDns() {
        return mEnableStaleDns;
    }

    @Nullable
    public Long getPersistHostCachePeriodMillis() {
        return mPersistHostCachePeriodMillis;
    }

    @Nullable
    public Boolean getPreestablishConnectionsToStaleDnsResults() {
        return mPreestablishConnectionsToStaleDnsResults;
    }

    @Nullable
    public StaleDnsOptions getStaleDnsOptions() {
        return mStaleDnsOptions;
    }

    public static Builder builder() {
        return new Builder();
    }

    /**
     * A class configuring Cronet's stale DNS functionality.
     *
     * <p>DNS resolution is one of the steps on the critical path to making a URL request, but it
     * can be slow for various reasons (underlying network latency, buffer bloat, packet loss,
     * etc.).
     *
     * <p>Depending on the use case, it might be worthwhile for an app developer to trade off
     * guaranteed DNS record freshness for better availability of DNS records.
     *
     * <p>Stale results can include both:
     *
     * <ul>
     *   <li>results returned from the current network's DNS server, but past their time-to-live,
     * and <li>results returned from a previous network's DNS server, whether expired or not.
     * </ul>
     */
    public static class StaleDnsOptions {
        @Nullable
        public Long getFreshLookupTimeoutMillis() {
            return mFreshLookupTimeoutMillis;
        }

        @Nullable
        public Long getMaxExpiredDelayMillis() {
            return mMaxExpiredDelayMillis;
        }

        @Nullable
        public Boolean getAllowCrossNetworkUsage() {
            return mAllowCrossNetworkUsage;
        }

        @Nullable
        public Boolean getUseStaleOnNameNotResolved() {
            return mUseStaleOnNameNotResolved;
        }

        public static Builder builder() {
            return new Builder();
        }

        @Nullable private final Long mFreshLookupTimeoutMillis;
        @Nullable private final Long mMaxExpiredDelayMillis;
        @Nullable private final Boolean mAllowCrossNetworkUsage;
        @Nullable private final Boolean mUseStaleOnNameNotResolved;

        StaleDnsOptions(Builder builder) {
            this.mFreshLookupTimeoutMillis = builder.mFreshLookupTimeoutMillis;
            this.mMaxExpiredDelayMillis = builder.mMaxExpiredDelayMillis;
            this.mAllowCrossNetworkUsage = builder.mAllowCrossNetworkUsage;
            this.mUseStaleOnNameNotResolved = builder.mUseStaleOnNameNotResolved;
        }

        /** Builder for {@link StaleDnsOptions}. */
        public static final class Builder {
            private Long mFreshLookupTimeoutMillis;
            private Long mMaxExpiredDelayMillis;
            private Boolean mAllowCrossNetworkUsage;
            private Boolean mUseStaleOnNameNotResolved;

            Builder() {}

            /**
             * Sets how long (in milliseconds) to wait for a DNS request to return before using a
             * stale result instead. If set to zero, returns stale results instantly but continues
             * the DNS request in the background to update the cache.
             *
             * @return the builder for chaining
             */
            public Builder setFreshLookupTimeoutMillis(long freshLookupTimeoutMillis) {
                this.mFreshLookupTimeoutMillis = freshLookupTimeoutMillis;
                return this;
            }

            /**
             * Same as {@link #setFreshLookupTimeoutMillis(long)} but using {@link
             * java.time.Duration}.
             *
             * @return the builder for chaining
             */
            @RequiresApi(VERSION_CODES.O)
            public Builder setFreshLookupTimeout(@NonNull Duration freshLookupTimeout) {
                Objects.requireNonNull(freshLookupTimeout);
                return setFreshLookupTimeoutMillis(freshLookupTimeout.toMillis());
            }

            /**
             * Sets how long (in milliseconds) past expiration to consider using expired results.
             * Setting the value to zero means expired records can be used indefinitely.
             *
             * @return the builder for chaining
             */
            public Builder setMaxExpiredDelayMillis(long maxExpiredDelayMillis) {
                this.mMaxExpiredDelayMillis = maxExpiredDelayMillis;
                return this;
            }

            /**
             * Same as {@link #setMaxExpiredDelayMillis(long)} but using {@link java.time.Duration}.
             *
             * @return the builder for chaining
             */
            @RequiresApi(VERSION_CODES.O)
            public Builder setMaxExpiredDelay(@NonNull Duration maxExpiredDelay) {
                Objects.requireNonNull(maxExpiredDelay);
                return setMaxExpiredDelayMillis(maxExpiredDelay.toMillis());
            }

            /**
             * Sets whether to return results originating from other networks or not. Normally,
             * Cronet clears the DNS cache entirely when switching connections, e.g. between two
             * Wi-Fi networks or from Wi-Fi to 4G.
             *
             * @return the builder for chaining
             */
            public Builder allowCrossNetworkUsage(boolean allowCrossNetworkUsage) {
                this.mAllowCrossNetworkUsage = allowCrossNetworkUsage;
                return this;
            }

            /**
             * Sets whether to allow use of stale DNS results when network resolver fails to resolve
             * the hostname.
             *
             * <p>Depending on the use case, if Cronet quickly sees a fresh failure, it may be
             * desirable to use the failure as it is technically the fresher result, and we had such
             * a fresh result quickly; or, prefer having any result (even if stale) to use over
             * having a failure.
             *
             * @return the builder for chaining
             */
            public Builder useStaleOnNameNotResolved(boolean useStaleOnNameNotResolved) {
                this.mUseStaleOnNameNotResolved = useStaleOnNameNotResolved;
                return this;
            }

            /**
             * Creates and returns the final {@link StaleDnsOptions} instance, based on the values
             * in this builder.
             */
            public StaleDnsOptions build() {
                return new StaleDnsOptions(this);
            }
        }
    }

    /** Builder for {@link DnsOptions}. */
    public static final class Builder {
        @Nullable private Boolean mUseBuiltInDnsResolver;
        @Nullable private Boolean mEnableStaleDns;
        @Nullable private StaleDnsOptions mStaleDnsOptions;
        @Nullable private Boolean mPersistHostCache;
        @Nullable private Long mPersistHostCachePeriodMillis;
        @Nullable private Boolean mPreestablishConnectionsToStaleDnsResults;

        Builder() {}

        public Builder useBuiltInDnsResolver(boolean enable) {
            this.mUseBuiltInDnsResolver = enable;
            return this;
        }

        /**
         * Sets whether to use stale DNS results at all.
         *
         * @return the builder for chaining
         */
        public Builder enableStaleDns(boolean enable) {
            this.mEnableStaleDns = enable;
            return this;
        }

        /**
         * Sets detailed configuration for stale DNS.
         *
         * Only relevant if {@link #enableStaleDns(boolean)} is set.
         *
         * @return this builder for chaining.
         */
        public Builder setStaleDnsOptions(StaleDnsOptions staleDnsOptions) {
            this.mStaleDnsOptions = staleDnsOptions;
            return this;
        }

        /** @see #setStaleDnsOptions(StaleDnsOptions) */
        @Experimental
        public Builder setStaleDnsOptions(StaleDnsOptions.Builder staleDnsOptionsBuilder) {
            return setStaleDnsOptions(staleDnsOptionsBuilder.build());
        }

        /**
         * Sets whether Cronet should use stale cached DNS records to pre-establish connections.
         *
         * <p>If enabled, Cronet will optimistically pre-establish connections to servers that
         * matched the hostname at some point in the past and were cached but the cache entry
         * expired. Such connections won't be used further until a new DNS lookup confirms the
         * cached record was up to date.
         *
         * <p>To use cached DNS records straight away, use {@link #enableStaleDns} and {@link
         * StaleDnsOptions} configuration options.
         *
         * <p>This option may not be available for all networking protocols.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder preestablishConnectionsToStaleDnsResults(boolean enable) {
            this.mPreestablishConnectionsToStaleDnsResults = enable;
            return this;
        }

        /**
         * Sets whether the DNS cache should be persisted to disk.
         *
         * <p>Only relevant if {@link CronetEngine.Builder#setStoragePath(String)} is
         * set.
         *
         * @return the builder for chaining
         */
        public Builder persistHostCache(boolean persistHostCache) {
            this.mPersistHostCache = persistHostCache;
            return this;
        }

        /**
         * Sets the minimum period between subsequent writes to disk for DNS cache persistence.
         *
         * <p>Only relevant if {@link #persistHostCache(boolean)} is set to true.
         *
         * @return the builder for chaining
         */
        public Builder setPersistHostCachePeriodMillis(long persistHostCachePeriodMillis) {
            this.mPersistHostCachePeriodMillis = persistHostCachePeriodMillis;
            return this;
        }

        /**
         * Same as {@link #setPersistHostCachePeriodMillis(long)} but using {@link
         * java.time.Duration}.
         *
         * @return the builder for chaining
         */
        @RequiresApi(api = VERSION_CODES.O)
        public Builder setPersistDelay(@NonNull Duration persistToDiskPeriod) {
            Objects.requireNonNull(persistToDiskPeriod);
            return setPersistHostCachePeriodMillis(persistToDiskPeriod.toMillis());
        }

        /**
         * Creates and returns the final {@link DnsOptions} instance, based on the values in this
         * builder.
         */
        public DnsOptions build() {
            return new DnsOptions(this);
        }
    }

    /**
     * An annotation for APIs which are not considered stable yet.
     *
     * <p>Experimental APIs are subject to change, breakage, or removal at any time and may not be
     * production ready.
     *
     * <p>It's highly recommended to reach out to Cronet maintainers
     * (<code>net-dev@chromium.org</code>) before using one of the APIs annotated as experimental
     * outside of debugging and proof-of-concept code.
     *
     * <p>By using an Experimental API, applications acknowledge that they are doing so at their own
     * risk.
     */
    @RequiresOptIn
    public @interface Experimental {}
}
