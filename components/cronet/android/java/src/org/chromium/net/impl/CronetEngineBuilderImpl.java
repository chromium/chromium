// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;
import static android.os.Process.THREAD_PRIORITY_LOWEST;

import android.content.Context;
import android.os.Process;
import android.os.SystemClock;
import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.CronetEngine;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.impl.CronetLogger.CronetSource;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.IDN;
import java.util.Date;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.regex.Pattern;

/** Implementation of {@link ICronetEngineBuilder}. */
public abstract class CronetEngineBuilderImpl extends ICronetEngineBuilder {
    /** A hint that a host supports QUIC. */
    public static class QuicHint {
        // The host.
        final String mHost;
        // Port of the server that supports QUIC.
        final int mPort;
        // Alternate protocol port.
        final int mAlternatePort;

        QuicHint(String host, int port, int alternatePort) {
            mHost = host;
            mPort = port;
            mAlternatePort = alternatePort;
        }
    }

    /** A public key pin. */
    public static class Pkp {
        // Host to pin for.
        final String mHost;
        // Array of SHA-256 hashes of keys.
        final byte[][] mHashes;
        // Should pin apply to subdomains?
        final boolean mIncludeSubdomains;
        // When the pin expires.
        final Date mExpirationDate;

        Pkp(String host, byte[][] hashes, boolean includeSubdomains, Date expirationDate) {
            mHost = host;
            mHashes = hashes;
            mIncludeSubdomains = includeSubdomains;
            mExpirationDate = expirationDate;
        }
    }

    /** Mapping between public builder view of HttpCacheMode and internal builder one. */
    @VisibleForTesting
    static enum HttpCacheMode {
        DISABLED(HttpCacheType.DISABLED, false),
        DISK(HttpCacheType.DISK, true),
        DISK_NO_HTTP(HttpCacheType.DISK, false),
        MEMORY(HttpCacheType.MEMORY, true);

        private final int mType;
        private final boolean mContentCacheEnabled;

        private HttpCacheMode(int type, boolean contentCacheEnabled) {
            mContentCacheEnabled = contentCacheEnabled;
            mType = type;
        }

        int getType() {
            return mType;
        }

        boolean isContentCacheEnabled() {
            return mContentCacheEnabled;
        }

        @HttpCacheSetting
        int toPublicBuilderCacheMode() {
            switch (this) {
                case DISABLED:
                    return CronetEngine.Builder.HTTP_CACHE_DISABLED;
                case DISK_NO_HTTP:
                    return CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP;
                case DISK:
                    return CronetEngine.Builder.HTTP_CACHE_DISK;
                case MEMORY:
                    return CronetEngine.Builder.HTTP_CACHE_IN_MEMORY;
                default:
                    throw new IllegalArgumentException("Unknown internal builder cache mode");
            }
        }

        @VisibleForTesting
        static HttpCacheMode fromPublicBuilderCacheMode(@HttpCacheSetting int cacheMode) {
            switch (cacheMode) {
                case CronetEngine.Builder.HTTP_CACHE_DISABLED:
                    return DISABLED;
                case CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP:
                    return DISK_NO_HTTP;
                case CronetEngine.Builder.HTTP_CACHE_DISK:
                    return DISK;
                case CronetEngine.Builder.HTTP_CACHE_IN_MEMORY:
                    return MEMORY;
                default:
                    throw new IllegalArgumentException("Unknown public builder cache mode");
            }
        }
    }

    private static final Pattern INVALID_PKP_HOST_NAME = Pattern.compile("^[0-9\\.]*$");

    private static final int INVALID_THREAD_PRIORITY = THREAD_PRIORITY_LOWEST + 1;

    @VisibleForTesting
    static int sApiLevel = VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel();

    protected final CronetLogger mLogger;

    // Private fields are simply storage of configuration for the resulting CronetEngine.
    // See setters below for verbose descriptions.
    private final Context mApplicationContext;
    private final List<QuicHint> mQuicHints = new LinkedList<>();
    private final List<Pkp> mPkps = new LinkedList<>();
    private final CronetSource mSource;
    private boolean mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    private String mUserAgent;
    private String mStoragePath;
    private boolean mQuicEnabled;
    private boolean mHttp2Enabled;
    private boolean mBrotiEnabled;
    private HttpCacheMode mHttpCacheMode;
    private long mHttpCacheMaxSize;
    private String mExperimentalOptions;
    protected long mMockCertVerifier;
    private boolean mNetworkQualityEstimatorEnabled;
    private int mThreadPriority = INVALID_THREAD_PRIORITY;

    /**
     * Default config enables SPDY and QUIC, disables SDCH and HTTP cache.
     *
     * @param context Android {@link Context} for engine to use.
     */
    public CronetEngineBuilderImpl(Context context, CronetSource source) {
        var startUptimeMillis = SystemClock.uptimeMillis();
        boolean successful = false;
        mApplicationContext = context.getApplicationContext();
        mSource = source;
        mLogger = CronetLoggerFactory.createLogger(mApplicationContext, mSource);
        try {
            enableQuic(true);
            enableHttp2(true);
            enableBrotli(false);
            enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISABLED, 0);
            enableNetworkQualityEstimator(false);
            enablePublicKeyPinningBypassForLocalTrustAnchors(true);

            successful = true;
        } finally {
            maybeLogCronetEngineBuilderInitializedInfo(startUptimeMillis, successful);
        }
    }

    /** TODO(b/332878149): Remove once this has landed internally and we've fixed all failures. */
    public CronetEngineBuilderImpl(Context context) {
        this(context, CronetSource.CRONET_SOURCE_UNSPECIFIED);
    }

    private void maybeLogCronetEngineBuilderInitializedInfo(
            long startUptimeMillis, boolean successful) {
        // Normally, the API code is responsible for logging this. However this only happens if the
        // app is bundling an API jar that is recent enough to include the logging code. If it does
        // not, we are on the hook for doing the logging here in impl code.
        //
        // The addition of logging code to the API was accompanied by an API level bump so that we
        // can detect this case.
        if (sApiLevel >= 30) return;

        var logInfo = new CronetLogger.CronetEngineBuilderInitializedInfo();
        logInfo.creationSuccessful = false;
        try {
            logInfo.author = CronetLogger.CronetEngineBuilderInitializedInfo.Author.IMPL;
            logInfo.uid = Process.myUid();
            logInfo.implVersion = new CronetLogger.CronetVersion(ImplVersion.getCronetVersion());
            logInfo.source = mSource;
            logInfo.apiVersion =
                    new CronetLogger.CronetVersion(
                            VersionSafeCallbacks.ApiVersion.getCronetVersion());
            logInfo.cronetInitializationRef = getLogCronetInitializationRef();
            logInfo.creationSuccessful = successful;
        } finally {
            logInfo.engineBuilderCreatedLatencyMillis =
                    (int) (SystemClock.uptimeMillis() - startUptimeMillis);
            mLogger.logCronetEngineBuilderInitializedInfo(logInfo);
        }
    }

    CronetSource getCronetSource() {
        return mSource;
    }

    @Override
    public String getDefaultUserAgent() {
        return UserAgent.from(mApplicationContext);
    }

    @Override
    public CronetEngineBuilderImpl setUserAgent(String userAgent) {
        mUserAgent = userAgent;
        return this;
    }

    @VisibleForTesting
    String getUserAgent() {
        return mUserAgent;
    }

    @Override
    public CronetEngineBuilderImpl setStoragePath(String value) {
        if (!new File(value).isDirectory()) {
            throw new IllegalArgumentException("Storage path must be set to existing directory");
        }
        mStoragePath = value;
        return this;
    }

    @VisibleForTesting
    String storagePath() {
        return mStoragePath;
    }

    @Override
    public CronetEngineBuilderImpl setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
        // |CronetEngineBuilderImpl| is an abstract class that is used by concrete builder
        // implementations, including the Java Cronet engine builder; therefore, the implementation
        // of this method should be "no-op". Subclasses that care about the library loader
        // should override this method.
        return this;
    }

    /**
     * Default implementation of the method that returns {@code null}.
     *
     * @return {@code null}.
     */
    VersionSafeCallbacks.LibraryLoader libraryLoader() {
        return null;
    }

    @Override
    public CronetEngineBuilderImpl enableQuic(boolean value) {
        mQuicEnabled = value;
        return this;
    }

    @VisibleForTesting
    boolean quicEnabled() {
        return mQuicEnabled;
    }

    /**
     * Constructs default QUIC User Agent Id string including application name
     * and Cronet version. Returns empty string if QUIC is not enabled.
     *
     * @return QUIC User Agent ID string.
     */
    String getDefaultQuicUserAgentId() {
        return mQuicEnabled ? UserAgent.getQuicUserAgentIdFrom(mApplicationContext) : "";
    }

    @Override
    public CronetEngineBuilderImpl enableHttp2(boolean value) {
        mHttp2Enabled = value;
        return this;
    }

    @VisibleForTesting
    boolean http2Enabled() {
        return mHttp2Enabled;
    }

    @Override
    public CronetEngineBuilderImpl enableSdch(boolean value) {
        return this;
    }

    @Override
    public CronetEngineBuilderImpl enableBrotli(boolean value) {
        mBrotiEnabled = value;
        return this;
    }

    @VisibleForTesting
    boolean brotliEnabled() {
        return mBrotiEnabled;
    }

    @IntDef({
        CronetEngine.Builder.HTTP_CACHE_DISABLED,
        CronetEngine.Builder.HTTP_CACHE_IN_MEMORY,
        CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP,
        CronetEngine.Builder.HTTP_CACHE_DISK
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HttpCacheSetting {}

    @Override
    public CronetEngineBuilderImpl enableHttpCache(@HttpCacheSetting int cacheMode, long maxSize) {
        HttpCacheMode cacheModeEnum = HttpCacheMode.fromPublicBuilderCacheMode(cacheMode);

        if (cacheModeEnum.getType() == HttpCacheType.DISK && storagePath() == null) {
            throw new IllegalArgumentException("Storage path must be set");
        }

        mHttpCacheMode = cacheModeEnum;
        mHttpCacheMaxSize = maxSize;

        return this;
    }

    boolean cacheDisabled() {
        return !mHttpCacheMode.isContentCacheEnabled();
    }

    long httpCacheMaxSize() {
        return mHttpCacheMaxSize;
    }

    @VisibleForTesting
    int httpCacheMode() {
        return mHttpCacheMode.getType();
    }

    @HttpCacheSetting
    int publicBuilderHttpCacheMode() {
        return mHttpCacheMode.toPublicBuilderCacheMode();
    }

    @Override
    public CronetEngineBuilderImpl addQuicHint(String host, int port, int alternatePort) {
        if (host.contains("/")) {
            throw new IllegalArgumentException("Illegal QUIC Hint Host: " + host);
        }
        mQuicHints.add(new QuicHint(host, port, alternatePort));
        return this;
    }

    List<QuicHint> quicHints() {
        return mQuicHints;
    }

    @Override
    public CronetEngineBuilderImpl addPublicKeyPins(
            String hostName,
            Set<byte[]> pinsSha256,
            boolean includeSubdomains,
            Date expirationDate) {
        Objects.requireNonNull(hostName, "The hostname cannot be null.");
        Objects.requireNonNull(pinsSha256, "The set of SHA256 pins cannot be null.");
        Objects.requireNonNull(expirationDate, "The pin expiration date cannot be null.");

        String idnHostName = validateHostNameForPinningAndConvert(hostName);
        // Convert the pin to BASE64 encoding to remove duplicates.
        Map<String, byte[]> hashes = new HashMap<>();
        for (byte[] pinSha256 : pinsSha256) {
            if (pinSha256 == null || pinSha256.length != 32) {
                throw new IllegalArgumentException("Public key pin is invalid");
            }
            hashes.put(Base64.encodeToString(pinSha256, 0), pinSha256);
        }
        // Add new element to PKP list.
        mPkps.add(
                new Pkp(
                        idnHostName,
                        hashes.values().toArray(new byte[hashes.size()][]),
                        includeSubdomains,
                        expirationDate));
        return this;
    }

    /**
     * Returns list of public key pins.
     * @return list of public key pins.
     */
    List<Pkp> publicKeyPins() {
        return mPkps;
    }

    @Override
    public CronetEngineBuilderImpl enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mPublicKeyPinningBypassForLocalTrustAnchorsEnabled = value;
        return this;
    }

    @VisibleForTesting
    boolean publicKeyPinningBypassForLocalTrustAnchorsEnabled() {
        return mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
    }

    /**
     * Checks whether a given string represents a valid host name for PKP and converts it
     * to ASCII Compatible Encoding representation according to RFC 1122, RFC 1123 and
     * RFC 3490. This method is more restrictive than required by RFC 7469. Thus, a host
     * that contains digits and the dot character only is considered invalid.
     *
     * Note: Currently Cronet doesn't have native implementation of host name validation that
     *       can be used. There is code that parses a provided URL but doesn't ensure its
     *       correctness. The implementation relies on {@code getaddrinfo} function.
     *
     * @param hostName host name to check and convert.
     * @return true if the string is a valid host name.
     * @throws IllegalArgumentException if the the given string does not represent a valid
     *                                  hostname.
     */
    private static String validateHostNameForPinningAndConvert(String hostName)
            throws IllegalArgumentException {
        if (INVALID_PKP_HOST_NAME.matcher(hostName).matches()) {
            throw new IllegalArgumentException(
                    "Hostname "
                            + hostName
                            + " is illegal."
                            + " A hostname should not consist of digits and/or dots only.");
        }
        // Workaround for crash, see crbug.com/634914
        if (hostName.length() > 255) {
            throw new IllegalArgumentException(
                    "Hostname "
                            + hostName
                            + " is too long."
                            + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
        try {
            return IDN.toASCII(hostName, IDN.USE_STD3_ASCII_RULES);
        } catch (IllegalArgumentException ex) {
            throw new IllegalArgumentException(
                    "Hostname "
                            + hostName
                            + " is illegal."
                            + " The name of the host does not comply with RFC 1122 and RFC 1123.");
        }
    }

    @Override
    public CronetEngineBuilderImpl setExperimentalOptions(String options) {
        mExperimentalOptions = options;
        return this;
    }

    public String experimentalOptions() {
        return mExperimentalOptions;
    }

    /**
     * Sets a native MockCertVerifier for testing. See
     * {@code MockCertVerifier.createMockCertVerifier} for a method that
     * can be used to create a MockCertVerifier.
     * @param mockCertVerifier pointer to native MockCertVerifier.
     * @return the builder to facilitate chaining.
     */
    public CronetEngineBuilderImpl setMockCertVerifierForTesting(long mockCertVerifier) {
        mMockCertVerifier = mockCertVerifier;
        return this;
    }

    long mockCertVerifier() {
        return mMockCertVerifier;
    }

    /**
     * @return true if the network quality estimator has been enabled for
     * this builder.
     */
    @VisibleForTesting
    boolean networkQualityEstimatorEnabled() {
        return mNetworkQualityEstimatorEnabled;
    }

    @Override
    public CronetEngineBuilderImpl enableNetworkQualityEstimator(boolean value) {
        mNetworkQualityEstimatorEnabled = value;
        return this;
    }

    @Override
    public CronetEngineBuilderImpl setThreadPriority(int priority) {
        if (priority > THREAD_PRIORITY_LOWEST || priority < -20) {
            throw new IllegalArgumentException("Thread priority invalid");
        }
        mThreadPriority = priority;
        return this;
    }

    @Override
    protected long getLogCronetInitializationRef() {
        return 0;
    }

    /**
     * @return thread priority provided by user, or {@code defaultThreadPriority} if none provided.
     */
    @VisibleForTesting
    int threadPriority(int defaultThreadPriority) {
        return mThreadPriority == INVALID_THREAD_PRIORITY ? defaultThreadPriority : mThreadPriority;
    }

    /**
     * Returns {@link Context} for builder.
     *
     * @return {@link Context} for builder.
     */
    Context getContext() {
        return mApplicationContext;
    }

    CronetLogger.CronetEngineBuilderInfo toLoggerInfo() {
        return new CronetLogger.CronetEngineBuilderInfo(
                /* publicKeyPinningBypassForLocalTrustAnchorsEnabled= */ publicKeyPinningBypassForLocalTrustAnchorsEnabled(),
                /* userAgent= */ getUserAgent(),
                /* storagePath= */ storagePath(),
                /* quicEnabled= */ quicEnabled(),
                /* http2Enabled= */ http2Enabled(),
                /* brotiEnabled= */ brotliEnabled(),
                /* httpCacheMode= */ publicBuilderHttpCacheMode(),
                /* experimentalOptions= */ experimentalOptions(),
                /* networkQualityEstimatorEnabled= */ networkQualityEstimatorEnabled(),
                /* threadPriority= */ threadPriority(THREAD_PRIORITY_BACKGROUND),
                /* cronetInitializationRef= */ getLogCronetInitializationRef());
    }
}
