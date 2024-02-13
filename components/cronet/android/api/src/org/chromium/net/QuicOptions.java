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
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.Objects;
import java.util.Set;

/**
 * Configuration options for QUIC in Cronet.
 *
 * <p>The settings in this class are only relevant if QUIC is enabled. Use {@link
 * org.chromium.net.CronetEngine.Builder#enableQuic(boolean)} to enable / disable QUIC for the
 * Cronet engine.
 */
public final class QuicOptions {
    private final Set<String> mQuicHostAllowlist;
    private final Set<String> mEnabledQuicVersions;

    private final Set<String> mConnectionOptions;
    private final Set<String> mClientConnectionOptions;
    @Nullable private final Integer mInMemoryServerConfigsCacheSize;
    @Nullable private final String mHandshakeUserAgent;
    @Nullable private final Boolean mRetryWithoutAltSvcOnQuicErrors;
    @Nullable private final Boolean mEnableTlsZeroRtt;

    @Nullable private final Long mPreCryptoHandshakeIdleTimeoutSeconds;
    @Nullable private final Long mCryptoHandshakeTimeoutSeconds;

    @Nullable private final Long mIdleConnectionTimeoutSeconds;
    @Nullable private final Long mRetransmittableOnWireTimeoutMillis;

    @Nullable private final Boolean mCloseSessionsOnIpChange;
    @Nullable private final Boolean mGoawaySessionsOnIpChange;

    @Nullable private final Long mInitialBrokenServicePeriodSeconds;
    @Nullable private final Boolean mIncreaseBrokenServicePeriodExponentially;
    @Nullable private final Boolean mDelayJobsWithAvailableSpdySession;

    private final Set<String> mExtraQuicheFlags;

    QuicOptions(Builder builder) {
        this.mQuicHostAllowlist =
                Collections.unmodifiableSet(new LinkedHashSet<>(builder.mQuicHostAllowlist));
        this.mEnabledQuicVersions =
                Collections.unmodifiableSet(new LinkedHashSet<>(builder.mEnabledQuicVersions));
        this.mConnectionOptions =
                Collections.unmodifiableSet(new LinkedHashSet<>(builder.mConnectionOptions));
        this.mClientConnectionOptions =
                Collections.unmodifiableSet(new LinkedHashSet<>(builder.mClientConnectionOptions));
        this.mInMemoryServerConfigsCacheSize = builder.mInMemoryServerConfigsCacheSize;
        this.mHandshakeUserAgent = builder.mHandshakeUserAgent;
        this.mRetryWithoutAltSvcOnQuicErrors = builder.mRetryWithoutAltSvcOnQuicErrors;
        this.mEnableTlsZeroRtt = builder.mEnableTlsZeroRtt;
        this.mPreCryptoHandshakeIdleTimeoutSeconds = builder.mPreCryptoHandshakeIdleTimeoutSeconds;
        this.mCryptoHandshakeTimeoutSeconds = builder.mCryptoHandshakeTimeoutSeconds;
        this.mIdleConnectionTimeoutSeconds = builder.mIdleConnectionTimeoutSeconds;
        this.mRetransmittableOnWireTimeoutMillis = builder.mRetransmittableOnWireTimeoutMillis;
        this.mCloseSessionsOnIpChange = builder.mCloseSessionsOnIpChange;
        this.mGoawaySessionsOnIpChange = builder.mGoawaySessionsOnIpChange;
        this.mInitialBrokenServicePeriodSeconds = builder.mInitialBrokenServicePeriodSeconds;
        this.mIncreaseBrokenServicePeriodExponentially =
                builder.mIncreaseBrokenServicePeriodExponentially;
        this.mDelayJobsWithAvailableSpdySession = builder.mDelayJobsWithAvailableSpdySession;
        this.mExtraQuicheFlags =
                Collections.unmodifiableSet(new LinkedHashSet<>(builder.mExtraQuicheFlags));
    }

    public Set<String> getQuicHostAllowlist() {
        return mQuicHostAllowlist;
    }

    public Set<String> getEnabledQuicVersions() {
        return mEnabledQuicVersions;
    }

    public Set<String> getConnectionOptions() {
        return mConnectionOptions;
    }

    public Set<String> getClientConnectionOptions() {
        return mClientConnectionOptions;
    }

    @Nullable
    public Integer getInMemoryServerConfigsCacheSize() {
        return mInMemoryServerConfigsCacheSize;
    }

    @Nullable
    public String getHandshakeUserAgent() {
        return mHandshakeUserAgent;
    }

    @Nullable
    public Boolean getRetryWithoutAltSvcOnQuicErrors() {
        return mRetryWithoutAltSvcOnQuicErrors;
    }

    @Nullable
    public Boolean getEnableTlsZeroRtt() {
        return mEnableTlsZeroRtt;
    }

    @Nullable
    public Long getPreCryptoHandshakeIdleTimeoutSeconds() {
        return mPreCryptoHandshakeIdleTimeoutSeconds;
    }

    @Nullable
    public Long getCryptoHandshakeTimeoutSeconds() {
        return mCryptoHandshakeTimeoutSeconds;
    }

    @Nullable
    public Long getIdleConnectionTimeoutSeconds() {
        return mIdleConnectionTimeoutSeconds;
    }

    @Nullable
    public Long getRetransmittableOnWireTimeoutMillis() {
        return mRetransmittableOnWireTimeoutMillis;
    }

    @Nullable
    public Boolean getCloseSessionsOnIpChange() {
        return mCloseSessionsOnIpChange;
    }

    @Nullable
    public Boolean getGoawaySessionsOnIpChange() {
        return mGoawaySessionsOnIpChange;
    }

    @Nullable
    public Long getInitialBrokenServicePeriodSeconds() {
        return mInitialBrokenServicePeriodSeconds;
    }

    @Nullable
    public Boolean getIncreaseBrokenServicePeriodExponentially() {
        return mIncreaseBrokenServicePeriodExponentially;
    }

    @Nullable
    public Boolean getDelayJobsWithAvailableSpdySession() {
        return mDelayJobsWithAvailableSpdySession;
    }

    public Set<String> getExtraQuicheFlags() {
        return mExtraQuicheFlags;
    }

    public static Builder builder() {
        return new Builder();
    }

    /** Builder for {@link QuicOptions}. */
    public static class Builder {
        private final Set<String> mQuicHostAllowlist = new LinkedHashSet<>();
        private final Set<String> mEnabledQuicVersions = new LinkedHashSet<>();
        private final Set<String> mConnectionOptions = new LinkedHashSet<>();
        private final Set<String> mClientConnectionOptions = new LinkedHashSet<>();
        @Nullable private Integer mInMemoryServerConfigsCacheSize;
        @Nullable private String mHandshakeUserAgent;
        @Nullable private Boolean mRetryWithoutAltSvcOnQuicErrors;
        @Nullable private Boolean mEnableTlsZeroRtt;
        @Nullable private Long mPreCryptoHandshakeIdleTimeoutSeconds;
        @Nullable private Long mCryptoHandshakeTimeoutSeconds;
        @Nullable private Long mIdleConnectionTimeoutSeconds;
        @Nullable private Long mRetransmittableOnWireTimeoutMillis;
        @Nullable private Boolean mCloseSessionsOnIpChange;
        @Nullable private Boolean mGoawaySessionsOnIpChange;
        @Nullable private Long mInitialBrokenServicePeriodSeconds;
        @Nullable private Boolean mIncreaseBrokenServicePeriodExponentially;
        @Nullable private Boolean mDelayJobsWithAvailableSpdySession;
        @Nullable private final Set<String> mExtraQuicheFlags = new LinkedHashSet<>();

        Builder() {}

        /**
         * Adds a host to the QUIC allowlist.
         *
         * <p>If no hosts are specified, the per-host allowlist functionality is disabled.
         * Otherwise, Cronet will only use QUIC when talking to hosts on the allowlist.
         *
         * @return the builder for chaining
         */
        public Builder addAllowedQuicHost(String quicHost) {
            mQuicHostAllowlist.add(quicHost);
            return this;
        }

        /**
         * Adds a QUIC version to the list of QUIC versions to enable.
         *
         * <p>If no versions are specified, Cronet will use a list of default QUIC versions.
         *
         * <p>The version format is specified by
         * <a
         * href="https://github.com/google/quiche/blob/main/quiche/quic/core/quic_versions.cc#L344">QUICHE</a>.
         * Outside of filtering out values known to be obsolete, Cronet doesn't process the versions
         * anyhow and simply passes them along to QUICHE.
         *
         * @return the builder for chaining
         */
        @QuichePassthroughOption
        public Builder addEnabledQuicVersion(String enabledQuicVersion) {
            mEnabledQuicVersions.add(enabledQuicVersion);
            return this;
        }

        /**
         * Adds a QUIC tag to send in a QUIC handshake's connection options.
         *
         * <p>The QUIC tags should be presented as strings up to four letters long
         * (for instance, {@code NBHD}).
         *
         * <p>As the QUIC tags are under active development and some are only relevant to the
         * server, Cronet doesn't attempt to maintain a complete list of all supported QUIC flags as
         * a part of the API. The flags. Flags supported by QUICHE, a QUIC implementation used by
         * Cronet and Google servers, can be found <a
         * href=https://github.com/google/quiche/blob/main/quiche/quic/core/crypto/crypto_protocol.h">here</a>.
         *
         * @return the builder for chaining
         */
        @QuichePassthroughOption
        public Builder addConnectionOption(String connectionOption) {
            mConnectionOptions.add(connectionOption);
            return this;
        }

        /**
         * Adds a QUIC tag to send in a QUIC handshake's connection options that only affects
         * the client.
         *
         * <p>See {@link #addConnectionOption(String)} for more details.
         */
        @QuichePassthroughOption
        public Builder addClientConnectionOption(String clientConnectionOption) {
            mClientConnectionOptions.add(clientConnectionOption);
            return this;
        }

        /**
         * Sets how many server configurations (metadata like list of alt svc, whether QUIC is
         * supported, etc.) should be held in memory.
         *
         * <p>If the storage path is set ({@link
         * org.chromium.net.CronetEngine.Builder#setStoragePath(String)}, Cronet will also persist
         * the server configurations on disk.
         *
         * @return the builder for chaining
         */
        public Builder setInMemoryServerConfigsCacheSize(int inMemoryServerConfigsCacheSize) {
            this.mInMemoryServerConfigsCacheSize = inMemoryServerConfigsCacheSize;
            return this;
        }

        /**
         * Sets the user agent to be used outside of HTTP requests (for example for QUIC
         * handshakes).
         *
         * <p>To set the default user agent for HTTP requests, use
         * {@link CronetEngine.Builder#setUserAgent(String)} instead.
         *
         * @return the builder for chaining
         */
        public Builder setHandshakeUserAgent(String handshakeUserAgent) {
            this.mHandshakeUserAgent = handshakeUserAgent;
            return this;
        }

        /**
         * Sets whether requests that failed with a QUIC protocol errors should be retried without
         * using any {@code alt-svc} servers.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder retryWithoutAltSvcOnQuicErrors(boolean retryWithoutAltSvcOnQuicErrors) {
            this.mRetryWithoutAltSvcOnQuicErrors = retryWithoutAltSvcOnQuicErrors;
            return this;
        }

        /**
         * Sets whether TLS with 0-RTT should be enabled.
         *
         * <p>0-RTT is a performance optimization avoiding an extra round trip when resuming
         * connections to a known server.
         *
         * @see <a href="https://blog.cloudflare.com/introducing-0-rtt/">Cloudflare's 0-RTT
         *         blogpost</a>
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder enableTlsZeroRtt(boolean enableTlsZeroRtt) {
            this.mEnableTlsZeroRtt = enableTlsZeroRtt;
            return this;
        }

        /**
         * Sets the maximum idle time for a connection which hasn't completed a SSL handshake yet.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder setPreCryptoHandshakeIdleTimeoutSeconds(
                long preCryptoHandshakeIdleTimeoutSeconds) {
            this.mPreCryptoHandshakeIdleTimeoutSeconds = preCryptoHandshakeIdleTimeoutSeconds;
            return this;
        }

        /**
         * Sets the timeout for a connection SSL handshake.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder setCryptoHandshakeTimeoutSeconds(long cryptoHandshakeTimeoutSeconds) {
            this.mCryptoHandshakeTimeoutSeconds = cryptoHandshakeTimeoutSeconds;
            return this;
        }

        /**
         * Sets the maximum idle time for a connection. The actual value for the idle timeout is the
         * minimum of this value and the server's and is negotiated during the handshake. Thus, it
         * only applies after the handshake has completed. If no activity is detected on the
         * connection for the set duration, the connection is closed.
         *
         * <p>See <a href="https://www.rfc-editor.org/rfc/rfc9114.html#name-idle-connections">RFC
         * 9114, section 5.1 </a> for more details.
         *
         * @return the builder for chaining
         */
        public Builder setIdleConnectionTimeoutSeconds(long idleConnectionTimeoutSeconds) {
            this.mIdleConnectionTimeoutSeconds = idleConnectionTimeoutSeconds;
            return this;
        }

        /**
         * Same as {@link #setIdleConnectionTimeoutSeconds(long)} but using {@link
         * java.time.Duration}.
         *
         * @return the builder for chaining
         */
        @RequiresApi(VERSION_CODES.O)
        public Builder setIdleConnectionTimeout(@NonNull Duration idleConnectionTimeout) {
            Objects.requireNonNull(idleConnectionTimeout);
            return setIdleConnectionTimeoutSeconds(idleConnectionTimeout.toSeconds());
        }

        /**
         * Sets the maximum desired time between packets on wire.
         *
         * <p>When the retransmittable-on-wire time is exceeded Cronet will probe quality of the
         * network using artificial traffic. Smaller timeouts will typically  result in faster
         * discovery of a broken or degrading path, but also larger usage of resources (battery,
         * data).
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder setRetransmittableOnWireTimeoutMillis(
                long retransmittableOnWireTimeoutMillis) {
            this.mRetransmittableOnWireTimeoutMillis = retransmittableOnWireTimeoutMillis;
            return this;
        }

        /**
         * Sets whether QUIC sessions should be closed on IP address change.
         *
         * <p>Don't use in combination with connection migration
         * (configured using {@link ConnectionMigrationOptions}).
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder closeSessionsOnIpChange(boolean closeSessionsOnIpChange) {
            this.mCloseSessionsOnIpChange = closeSessionsOnIpChange;
            return this;
        }

        /**
         * Sets whether QUIC sessions should be goaway'd on IP address change.
         *
         * <p>Don't use in combination with connection migration
         * (configured using {@link ConnectionMigrationOptions}).
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder goawaySessionsOnIpChange(boolean goawaySessionsOnIpChange) {
            this.mGoawaySessionsOnIpChange = goawaySessionsOnIpChange;
            return this;
        }

        /**
         * Sets the initial for which Cronet shouldn't attempt to use QUIC for a given server after
         * the server's QUIC support turned out to be broken.
         *
         * <p>Once Cronet detects that a server advertises QUIC but doesn't actually speak it, it
         * marks the server as broken and doesn't attempt to use QUIC when talking to the server for
         * an amount of time. Once Cronet is past this point it will try using QUIC again. This is
         * to balance short term (there's no point wasting resources to try QUIC if the server is
         * broken) and long term (the breakage might have been temporary, using QUIC is generally
         * beneficial) interests.
         *
         * <p>The delay is increased every unsuccessful consecutive retry. See
         * {@link #increaseBrokenServicePeriodExponentially(boolean)} for details.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder setInitialBrokenServicePeriodSeconds(
                long initialBrokenServicePeriodSeconds) {
            this.mInitialBrokenServicePeriodSeconds = initialBrokenServicePeriodSeconds;
            return this;
        }

        /**
         * Sets whether the broken server period should scale exponentially.
         *
         * <p>If set to true, the initial delay (configurable
         * by {@link #setInitialBrokenServicePeriodSeconds}) will be scaled exponentially for
         * subsequent retries ({@code SCALING_FACTOR^NUM_TRIES * delay}). If false, the delay will
         * scale linearly (SCALING_FACTOR * NUM_TRIES * delay).
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder increaseBrokenServicePeriodExponentially(
                boolean increaseBrokenServicePeriodExponentially) {
            this.mIncreaseBrokenServicePeriodExponentially =
                    increaseBrokenServicePeriodExponentially;
            return this;
        }

        /**
         * Sets whether Cronet should wait for the primary path (usually QUIC) to be ready even if
         * there's a secondary path of reaching the server (SPDY / HTTP2) which is ready
         * immediately.
         *
         * @return the builder for chaining
         */
        @Experimental
        public Builder delayJobsWithAvailableSpdySession(
                boolean delayJobsWithAvailableSpdySession) {
            this.mDelayJobsWithAvailableSpdySession = delayJobsWithAvailableSpdySession;
            return this;
        }

        /**
         * Sets an arbitrary QUICHE flag. Flags should be passed in {@code FLAG_NAME=FLAG_VALUE}
         * format.
         *
         * See the <a href="https://github.com/google/quiche/">QUICHE code base</a> for a full list
         * of flags.
         *
         * @return the builder for chaining
         */
        @QuichePassthroughOption
        public Builder addExtraQuicheFlag(String extraQuicheFlag) {
            this.mExtraQuicheFlags.add(extraQuicheFlag);
            return this;
        }

        /**
         * Creates and returns the final {@link QuicOptions} instance, based on the values
         * in this builder.
         */
        public QuicOptions build() {
            return new QuicOptions(this);
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

    /**
     * An annotation for APIs which configure QUICHE options not curated by Cronet.
     *
     * <p>APIs annotated by this are considered stable from Cronet's perspective. However, they
     * simply pass the configuration options to QUICHE, a library that provides the HTTP3
     * implementation. As the dependency is under active development those flags might change
     * behavior, or get deleted. The application accepts the stability contract as stated by QUICHE.
     * Cronet is just a mediator passing the messages back and forth.
     *
     * <p>Cronet provides the APIs as a compromise between customer development velocity (some
     * customers value access to bleeding edge QUICHE features ASAP), and Cronet's own interests
     * (stability and readability of the API, capacity to propagate new QUICHE changes). Most Cronet
     * customers shouldn't need to use those APIs directly. Mature QUICHE features that are
     * generally useful will be exposed by Cronet as proper top level APIs or configuration options.
     */
    @RequiresOptIn
    public @interface QuichePassthroughOption {}
}
