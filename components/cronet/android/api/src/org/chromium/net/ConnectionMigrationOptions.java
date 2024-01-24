// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresOptIn;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A class configuring Cronet's connection migration functionality.
 *
 * <p>Connection migration stops open connections to servers from being destroyed when the
 * client device switches its L4 connectivity (typically the IP address as a result of using
 * a different network). This is particularly common with mobile devices losing
 * wifi connectivity and switching to cellular data, or vice versa (a.k.a. the parking lot
 * problem). QUIC uses connection identifiers which are independent of the underlying
 * transport layer to make this possible. If the client connects to a new network and wants
 * to preserve the existing connection, they can do so by using a connection identifier the server
 * knows to be a continuation of the existing connection.
 *
 * <p>The features are only available for QUIC connections and the server needs to support
 * connection migration.
 *
 * @see <a href="https://www.rfc-editor.org/rfc/rfc9000.html#section-9">Connection
 *     Migration specification</a>
 */
public class ConnectionMigrationOptions {
    /** Option is unspecified, platform default value will be used. */
    public static final int MIGRATION_OPTION_UNSPECIFIED = 0;

    /** Option is enabled. */
    public static final int MIGRATION_OPTION_ENABLED = 1;

    /** Option is disabled. */
    public static final int MIGRATION_OPTION_DISABLED = 2;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
            flag = false,
            value = {
                MIGRATION_OPTION_UNSPECIFIED,
                MIGRATION_OPTION_ENABLED,
                MIGRATION_OPTION_DISABLED,
            })
    @interface State {}

    private final @State int mEnableDefaultNetworkMigration;
    private final @State int mEnablePathDegradationMigration;
    private final @State int mAllowServerMigration;
    private final @State int mMigrateIdleConnections;
    @Nullable private final Long mIdleMigrationPeriodSeconds;
    private final @State int mRetryPreHandshakeErrorsOnAlternateNetwork;
    private final @State int mAllowNonDefaultNetworkUsage;
    @Nullable private final Long mMaxTimeOnNonDefaultNetworkSeconds;
    @Nullable private final Integer mMaxWriteErrorNonDefaultNetworkMigrationsCount;
    @Nullable private final Integer mMaxPathDegradingNonDefaultNetworkMigrationsCount;

    public @State int getDefaultNetworkMigration() {
        return mEnableDefaultNetworkMigration;
    }

    public @State int getPathDegradationMigration() {
        return mEnablePathDegradationMigration;
    }

    public @State int getAllowServerMigration() {
        return mAllowServerMigration;
    }

    public @State int getMigrateIdleConnections() {
        return mMigrateIdleConnections;
    }

    @Nullable
    public Long getIdleMigrationPeriodSeconds() {
        return mIdleMigrationPeriodSeconds;
    }

    public @State int getRetryPreHandshakeErrorsOnAlternateNetwork() {
        return mRetryPreHandshakeErrorsOnAlternateNetwork;
    }

    public @State int getAllowNonDefaultNetworkUsage() {
        return mAllowNonDefaultNetworkUsage;
    }

    @Nullable
    public Long getMaxTimeOnNonDefaultNetworkSeconds() {
        return mMaxTimeOnNonDefaultNetworkSeconds;
    }

    @Nullable
    public Integer getMaxWriteErrorNonDefaultNetworkMigrationsCount() {
        return mMaxWriteErrorNonDefaultNetworkMigrationsCount;
    }

    @Nullable
    public Integer getMaxPathDegradingNonDefaultNetworkMigrationsCount() {
        return mMaxPathDegradingNonDefaultNetworkMigrationsCount;
    }

    public ConnectionMigrationOptions(Builder builder) {
        this.mEnableDefaultNetworkMigration = builder.mEnableDefaultNetworkConnectionMigration;
        this.mEnablePathDegradationMigration = builder.mEnablePathDegradationMigration;
        this.mAllowServerMigration = builder.mAllowServerMigration;
        this.mMigrateIdleConnections = builder.mMigrateIdleConnections;
        this.mIdleMigrationPeriodSeconds = builder.mIdleConnectionMigrationPeriodSeconds;
        this.mRetryPreHandshakeErrorsOnAlternateNetwork =
                builder.mRetryPreHandshakeErrorsOnAlternateNetwork;
        this.mAllowNonDefaultNetworkUsage = builder.mAllowNonDefaultNetworkUsage;
        this.mMaxTimeOnNonDefaultNetworkSeconds = builder.mMaxTimeOnNonDefaultNetworkSeconds;
        this.mMaxWriteErrorNonDefaultNetworkMigrationsCount =
                builder.mMaxWriteErrorNonDefaultNetworkMigrationsCount;
        this.mMaxPathDegradingNonDefaultNetworkMigrationsCount =
                builder.mMaxPathDegradingNonDefaultMigrationsCount;
    }

    /** Builder for {@link ConnectionMigrationOptions}. */
    public static class Builder {
        private @State int mEnableDefaultNetworkConnectionMigration;
        private @State int mEnablePathDegradationMigration;
        private @State int mAllowServerMigration;
        private @State int mMigrateIdleConnections;
        @Nullable private Long mIdleConnectionMigrationPeriodSeconds;
        private @State int mRetryPreHandshakeErrorsOnAlternateNetwork;
        private @State int mAllowNonDefaultNetworkUsage;
        @Nullable private Long mMaxTimeOnNonDefaultNetworkSeconds;
        @Nullable private Integer mMaxWriteErrorNonDefaultNetworkMigrationsCount;
        @Nullable private Integer mMaxPathDegradingNonDefaultMigrationsCount;

        Builder() {}

        /**
         * Sets whether to enable the possibility of migrating connections on default network
         * change. If enabled, active QUIC connections will be migrated onto the new network when
         * the platform indicates that the default network is changing.
         *
         * @see <a href="https://developer.android.com/training/basics/network-ops/reading-network-state#listening-events">Android
         *     Network State</a>
         * @param defaultNetworkMigration Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        public Builder setDefaultNetworkMigration(@State int defaultNetworkMigration) {
            this.mEnableDefaultNetworkConnectionMigration = defaultNetworkMigration;
            return this;
        }

        /**
         * Sets whether to enable the possibility of migrating connections if the current path is
         * performing poorly.
         *
         * <p>Depending on other configuration, this can result to migrating the connections within
         * the same default network, or to a non-default network.
         *
         * @see #setAllowNonDefaultNetworkUsage(int)
         * @param pathDegradationMigration Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        public Builder setPathDegradationMigration(@State int pathDegradationMigration) {
            this.mEnablePathDegradationMigration = pathDegradationMigration;
            return this;
        }

        /**
         * Sets whether to enable the possibility of migrating connections to an alternate server
         * address at the server's request.
         *
         * @param allowServerMigration Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        @Experimental
        public Builder setAllowServerMigration(@State int allowServerMigration) {
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
         * @param migrateIdleConnections Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMigrateIdleConnections(@State int migrateIdleConnections) {
            this.mMigrateIdleConnections = migrateIdleConnections;
            return this;
        }

        /**
         * Sets the maximum idle period for which connections will still be migrated, in seconds.
         * The setting is shared for all connection migration types.
         *
         * <p>Only relevant if {@link #setMigrateIdleConnections(int)} is enabled.
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
         * Sets whether connections can be migrated to an alternate network when Cronet detects a
         * degradation of the path currently in use.
         *
         * <p>Note: This setting can result in requests being sent on non-default metered networks.
         * Make sure you're using metered networks sparingly, and fine tune parameters like {@link
         * #setMaxPathDegradingNonDefaultNetworkMigrationsCount(int)} and {@link
         * #setMaxTimeOnNonDefaultNetworkSeconds} to limit the time on non-default networks.
         *
         * @param allowNonDefaultNetworkUsage Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        @Experimental
        public Builder setAllowNonDefaultNetworkUsage(@State int allowNonDefaultNetworkUsage) {
            this.mAllowNonDefaultNetworkUsage = allowNonDefaultNetworkUsage;
            return this;
        }

        /**
         * Sets the maximum period for which eagerly migrated connections should remain on the
         * non-default network before they're migrated back. This time is not cumulative - each
         * migration off the default network for each connection measures and compares to this value
         * separately.
         *
         * <p>Only relevant if {@link #setAllowNonDefaultNetworkUsage(int)} is enabled.
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
         * <p>Only relevant if {@link #setAllowNonDefaultNetworkUsage(int)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMaxWriteErrorNonDefaultNetworkMigrationsCount(
                int maxWriteErrorEagerMigrationsCount) {
            this.mMaxWriteErrorNonDefaultNetworkMigrationsCount = maxWriteErrorEagerMigrationsCount;
            return this;
        }

        /**
         * Sets the maximum number of migrations to the non-default network upon encountering path
         * degradation for the existing connection. Counted cumulatively per network per connection.
         *
         * <p>Only relevant if {@link #setAllowNonDefaultNetworkUsage(int)} is enabled.
         *
         * @return this builder for chaining
         */
        @Experimental
        public Builder setMaxPathDegradingNonDefaultNetworkMigrationsCount(
                int maxPathDegradingEagerMigrationsCount) {
            this.mMaxPathDegradingNonDefaultMigrationsCount = maxPathDegradingEagerMigrationsCount;
            return this;
        }

        /**
         * Sets whether connections with pre-handshake errors should be retried on an alternative
         * network.
         *
         * <p>If true, a new connection may be established an alternate network if it fails on the
         * default network before handshake is confirmed.
         *
         * <p>Note: similarly to {@link #setAllowNonDefaultNetworkUsage(int)} this setting can
         * result in requests being sent on non-default metered networks. Use with caution!
         *
         * @param retryPreHandshakeErrorsOnAlternateNetwork Must be one of {@link
         *     ConnectionMigrationOptions#MIGRATION_OPTION_DISABLED MIGRATION_OPTION_*}.
         * @return this builder for chaining
         */
        @Experimental
        public Builder setRetryPreHandshakeErrorsOnNonDefaultNetwork(
                @State int retryPreHandshakeErrorsOnAlternateNetwork) {
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
