// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresOptIn;

/**
 * A class configuring Cronet's connection migration functionality.
 *
 * <p>Connection migration stops open connections to servers from being destroyed when the client
 * device switches its L4 connectivity (typically the IP address as a result of using a different
 * network). This is particularly common with mobile devices losing wifi connectivity and switching
 * to cellular data, or vice versa (a.k.a. the parking lot problem). QUIC uses connection
 * identifiers which are independent of the underlying transport layer to make this possible. If the
 * client connects to a new network and wants to preserve the existing connection, they can do so by
 * using a connection identifier the server knows to be a continuation of the existing connection.
 *
 * <p>The features are only available for QUIC connections and the server needs to support
 * connection migration.
 *
 * @see <a href="https://www.rfc-editor.org/rfc/rfc9000.html#section-9">Connection Migration
 *     specification</a>
 */
public final class ConnectionMigrationOptions {
    @Nullable private final Boolean mEnableDefaultNetworkMigration;
    @Nullable private final Boolean mEnablePathDegradationMigration;
    @Nullable private final Boolean mAllowServerMigration;
    @Nullable private final Boolean mMigrateIdleConnections;
    @Nullable private final Long mIdleMigrationPeriodSeconds;
    @Nullable private final Boolean mRetryPreHandshakeErrorsOnAlternateNetwork;
    @Nullable private final Boolean mAllowNonDefaultNetworkUsage;
    @Nullable private final Long mMaxTimeOnNonDefaultNetworkSeconds;
    @Nullable private final Integer mMaxWriteErrorEagerMigrationsCount;
    @Nullable private final Integer mMaxPathDegradingEagerMigrationsCount;

    @Nullable
    public Boolean getEnableDefaultNetworkMigration() {
        return mEnableDefaultNetworkMigration;
    }

    @Nullable
    public Boolean getEnablePathDegradationMigration() {
        return mEnablePathDegradationMigration;
    }

    @Nullable
    public Boolean getAllowServerMigration() {
        return mAllowServerMigration;
    }

    @Nullable
    public Boolean getMigrateIdleConnections() {
        return mMigrateIdleConnections;
    }

    @Nullable
    public Long getIdleMigrationPeriodSeconds() {
        return mIdleMigrationPeriodSeconds;
    }

    @Nullable
    public Boolean getRetryPreHandshakeErrorsOnAlternateNetwork() {
        return mRetryPreHandshakeErrorsOnAlternateNetwork;
    }

    @Nullable
    public Boolean getAllowNonDefaultNetworkUsage() {
        return mAllowNonDefaultNetworkUsage;
    }

    @Nullable
    public Long getMaxTimeOnNonDefaultNetworkSeconds() {
        return mMaxTimeOnNonDefaultNetworkSeconds;
    }

    @Nullable
    public Integer getMaxWriteErrorEagerMigrationsCount() {
        return mMaxWriteErrorEagerMigrationsCount;
    }

    @Nullable
    public Integer getMaxPathDegradingEagerMigrationsCount() {
        return mMaxPathDegradingEagerMigrationsCount;
    }

    private ConnectionMigrationOptions(Builder builder) {
        this.mEnableDefaultNetworkMigration = builder.mEnableDefaultNetworkConnectionMigration;
        this.mEnablePathDegradationMigration = builder.mEnablePathDegradationMigration;
        this.mAllowServerMigration = builder.mAllowServerMigration;
        this.mMigrateIdleConnections = builder.mMigrateIdleConnections;
        this.mIdleMigrationPeriodSeconds = builder.mIdleConnectionMigrationPeriodSeconds;
        this.mRetryPreHandshakeErrorsOnAlternateNetwork =
                builder.mRetryPreHandshakeErrorsOnAlternateNetwork;
        this.mAllowNonDefaultNetworkUsage = builder.mAllowNonDefaultNetworkUsage;
        this.mMaxTimeOnNonDefaultNetworkSeconds = builder.mMaxTimeOnNonDefaultNetworkSeconds;
        this.mMaxWriteErrorEagerMigrationsCount = builder.mMaxWriteErrorEagerMigrationsCount;
        this.mMaxPathDegradingEagerMigrationsCount = builder.mMaxPathDegradingEagerMigrationsCount;
    }

    /** Builder for {@link ConnectionMigrationOptions}. */
    public static class Builder {
        @Nullable private Boolean mEnableDefaultNetworkConnectionMigration;
        @Nullable private Boolean mEnablePathDegradationMigration;
        @Nullable private Boolean mAllowServerMigration;
        @Nullable private Boolean mMigrateIdleConnections;
        @Nullable private Long mIdleConnectionMigrationPeriodSeconds;
        @Nullable private Boolean mRetryPreHandshakeErrorsOnAlternateNetwork;
        @Nullable private Boolean mAllowNonDefaultNetworkUsage;
        @Nullable private Long mMaxTimeOnNonDefaultNetworkSeconds;
        @Nullable private Integer mMaxWriteErrorEagerMigrationsCount;
        @Nullable private Integer mMaxPathDegradingEagerMigrationsCount;

        private Builder() {}

        /**
         * Enables the possibility of migrating connections on default network change. If enabled,
         * active QUIC connections will be migrated onto the new network when the platform indicates
         * that the default network is changing.
         *
         * @see <a href="https://developer.android.com/training/basics/network-ops/reading-network-state#listening-events">Android
         *     Network State</a>
         *
         * @return this builder for chaining
         */
        public Builder enableDefaultNetworkMigration(
                boolean enableDefaultNetworkConnectionMigration) {
            this.mEnableDefaultNetworkConnectionMigration = enableDefaultNetworkConnectionMigration;
            return this;
        }

        /**
         * Enables the possibility of migrating connections if the current path is performing
         * poorly.
         *
         * <p>Depending on other configuration, this can result to migrating the connections within
         * the same default network, or to a non-default network.
         *
         * @see #allowNonDefaultNetworkUsage(boolean)
         *
         * @return this builder for chaining
         */
        public Builder enablePathDegradationMigration(boolean enable) {
            this.mEnablePathDegradationMigration = enable;
            return this;
        }

        /**
         * Enables the possibility of migrating connections to an alternate server address
         * at the server's request.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder allowServerMigration(boolean allowServerMigration) {
            this.mAllowServerMigration = allowServerMigration;
            return this;
        }

        /**
         * Configures whether migration of idle connections should be enabled or not.
         *
         * <p>If set to true, idle connections will be migrated too, as long as they haven't been
         * idle for too long. The setting is shared for all connection migration types. The maximum
         * idle period for which connections will still be migrated can be customized using {@link
         * #setIdleConnectionMigrationPeriodSeconds}.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder migrateIdleConnections(boolean migrateIdleConnections) {
            this.mMigrateIdleConnections = migrateIdleConnections;
            return this;
        }

        /**
         * Sets the maximum idle period for which connections will still be migrated, in seconds.
         * The setting is shared for all connection migration types.
         *
         * <p>Only relevant if {@link #migrateIdleConnections(boolean)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setIdleConnectionMigrationPeriodSeconds(
                long idleConnectionMigrationPeriodSeconds) {
            this.mIdleConnectionMigrationPeriodSeconds = idleConnectionMigrationPeriodSeconds;
            return this;
        }

        /**
         * Sets whether connections can be migrated to an alternate network when Cronet detects
         * a degradation of the path currently in use.
         *
         * <p>Note: This setting can result in requests being sent on non-default metered networks.
         * Make sure you're using metered networks sparingly, and fine tune parameters like
         * {@link #setMaxPathDegradingNonDefaultNetworkMigrationsCount(int)}
         * and {@link #setMaxTimeOnNonDefaultNetworkSeconds} to limit the time on non-default
         * networks.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder allowNonDefaultNetworkUsage(boolean enable) {
            this.mAllowNonDefaultNetworkUsage = enable;
            return this;
        }

        /**
         * Sets the maximum period for which eagerly migrated connections should remain on the
         * non-default network before they're migrated back. This time is not cumulative - each
         * migration off the default network for each connection measures and compares to this value
         * separately.
         *
         * <p>Only relevant if {@link #allowNonDefaultNetworkUsage(boolean)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMaxTimeOnNonDefaultNetworkSeconds(
                long maxTimeOnNonDefaultNetworkSeconds) {
            this.mMaxTimeOnNonDefaultNetworkSeconds = maxTimeOnNonDefaultNetworkSeconds;
            return this;
        }

        /**
         * Sets the maximum number of migrations to the non-default network upon encountering write
         * errors. Counted cumulatively per network per connection.
         *
         * <p>Only relevant if {@link #allowNonDefaultNetworkUsage(boolean)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMaxWriteErrorNonDefaultNetworkMigrationsCount(
                int maxWriteErrorEagerMigrationsCount) {
            this.mMaxWriteErrorEagerMigrationsCount = maxWriteErrorEagerMigrationsCount;
            return this;
        }

        /**
         * Sets the maximum number of migrations to the non-default network upon encountering path
         * degradation for the existing connection. Counted cumulatively per network per connection.
         *
         * <p>Only relevant if {@link #allowNonDefaultNetworkUsage(boolean)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMaxPathDegradingNonDefaultNetworkMigrationsCount(
                int maxPathDegradingEagerMigrationsCount) {
            this.mMaxPathDegradingEagerMigrationsCount = maxPathDegradingEagerMigrationsCount;
            return this;
        }

        /**
         * Sets whether connections with pre-handshake errors should be retried on an alternative
         * network.
         *
         * <p>If true, a new connection may be established an alternate network if it fails
         * on the default network before handshake is confirmed.
         *
         * <p>Note: similarly to {@link #allowNonDefaultNetworkUsage(boolean)} this setting can
         * result in requests being sent on non-default metered networks. Use with caution!
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder retryPreHandshakeErrorsOnNonDefaultNetwork(
                boolean retryPreHandshakeErrorsOnAlternateNetwork) {
            this.mRetryPreHandshakeErrorsOnAlternateNetwork =
                    retryPreHandshakeErrorsOnAlternateNetwork;
            return this;
        }

        /**
         * Creates and returns the final {@link ConnectionMigrationOptions} instance, based on the
         * values in this builder.
         */
        public ConnectionMigrationOptions build() {
            return new ConnectionMigrationOptions(this);
        }
    }

    public static Builder builder() {
        return new Builder();
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
