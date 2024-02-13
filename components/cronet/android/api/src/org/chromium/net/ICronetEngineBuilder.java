// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import java.util.Collections;
import java.util.Date;
import java.util.Set;

/**
 * Defines methods that the actual implementation of {@link CronetEngine.Builder} has to implement.
 * {@code CronetEngine.Builder} uses this interface to delegate the calls. For the documentation of
 * individual methods, please see the identically named methods in {@link
 * org.chromium.net.CronetEngine.Builder} and {@link
 * org.chromium.net.ExperimentalCronetEngine.Builder}.
 *
 * <p>{@hide internal class}
 */
public abstract class ICronetEngineBuilder {
    // The fields below list values which are known to getSupportedConfigOptions().
    //
    // Given the fields are final the constant value associated with them is compiled into
    // class using them. This makes it safe for all implementation to use the field in their code
    // and not worry about version skew (new implementation aware of values the old API is not),
    // as long as the values don't change meaning. This isn't true of enums and other dynamic
    // structures, hence we resort to plain old good ints.
    public static final int CONNECTION_MIGRATION_OPTIONS = 1;
    public static final int DNS_OPTIONS = 2;
    public static final int QUIC_OPTIONS = 3;

    // Public API methods.
    public abstract ICronetEngineBuilder addPublicKeyPins(
            String hostName,
            Set<byte[]> pinsSha256,
            boolean includeSubdomains,
            Date expirationDate);

    public abstract ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort);

    public abstract ICronetEngineBuilder enableHttp2(boolean value);

    public abstract ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize);

    public abstract ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(
            boolean value);

    public abstract ICronetEngineBuilder enableQuic(boolean value);

    public abstract ICronetEngineBuilder enableSdch(boolean value);

    public ICronetEngineBuilder enableBrotli(boolean value) {
        // Do nothing for older implementations.
        return this;
    }

    public ICronetEngineBuilder setQuicOptions(QuicOptions quicOptions) {
        return this;
    }

    public ICronetEngineBuilder setDnsOptions(DnsOptions dnsOptions) {
        return this;
    }

    public ICronetEngineBuilder setConnectionMigrationOptions(
            ConnectionMigrationOptions connectionMigrationOptions) {
        return this;
    }

    public abstract ICronetEngineBuilder setExperimentalOptions(String options);

    public abstract ICronetEngineBuilder setLibraryLoader(
            CronetEngine.Builder.LibraryLoader loader);

    public abstract ICronetEngineBuilder setStoragePath(String value);

    public abstract ICronetEngineBuilder setUserAgent(String userAgent);

    public abstract String getDefaultUserAgent();

    public abstract ExperimentalCronetEngine build();

    /**
     * Returns the set of configuration options the builder is able to support natively. This is
     * used internally to emulate newly added functionality using older APIs where possible.
     *
     * <p>The default implementation returns an empty set. Subclasses should override this method to
     * reflect the supported options that are applicable to them.
     */
    protected Set<Integer> getSupportedConfigOptions() {
        return Collections.emptySet();
    }

    // Experimental API methods.
    //
    // Note: all experimental API methods should have default implementation. This will allow
    // removing the experimental methods from the implementation layer without breaking
    // the client.

    public ICronetEngineBuilder enableNetworkQualityEstimator(boolean value) {
        return this;
    }

    public ICronetEngineBuilder setThreadPriority(int priority) {
        return this;
    }

    /**
     * Communicates the cronetInitializationRef for use in telemetry/logging, or 0 if the impl does
     * not support this method.
     *
     * <p>Cronet API code with API version level >=31 calls this method shortly after construction.
     */
    protected long getLogCronetInitializationRef() {
        return 0;
    }
}
