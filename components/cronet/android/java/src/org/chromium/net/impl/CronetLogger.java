// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static android.os.Process.THREAD_PRIORITY_BACKGROUND;

import java.time.Duration;

/**
 * Base class for implementing a CronetLogger.
 */
public abstract class CronetLogger {
    public static enum CronetSource {
        // Safe default, don't use explicitly.
        CRONET_SOURCE_UNSPECIFIED,
        // The library is bundled with the application.
        CRONET_SOURCE_STATICALLY_LINKED,
        // The library is loaded from GooglePlayServices.
        CRONET_SOURCE_PLAY_SERVICES,
        // The application is using the fallback implementation.
        CRONET_SOURCE_FALLBACK,
        // The library is loaded through the bootclasspath.
        CRONET_SOURCE_PLATFORM,
    }

    /**
     * Logs a cronetEngine creation action with the details of the creation.
     *
     * @param cronetEngineId the id of the engine being created.
     * @param engineBuilderInfo the configuration of the CronetEngine being created. See {@link
     *        CronetEngineBuilderInfo}
     * @param version the version of cronet used for the engine. See {@link CronetVersion}
     * @param source the source of the cronet provider for the engine. See {@link CronetSource}
     */
    public abstract void logCronetEngineCreation(int cronetEngineId,
            CronetEngineBuilderInfo engineBuilderInfo, CronetVersion version, CronetSource source);

    /**
     * Logs a request/response action.
     * @param cronetEngineId the id of the engine used for the request
     * @param trafficInfo the associated traffic information. See {@link CronetTrafficInfo}
     */
    public abstract void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo);

    /**
     * Aggregates the information about a CronetEngine configuration.
     */
    public static class CronetEngineBuilderInfo {
        private final boolean mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
        private final String mUserAgent;
        private final String mStoragePath;
        private final boolean mQuicEnabled;
        private final boolean mHttp2Enabled;
        private final boolean mBrotiEnabled;
        private final int mHttpCacheMode;
        private final String mExperimentalOptions;
        private final boolean mNetworkQualityEstimatorEnabled;
        private final int mThreadPriority;

        public CronetEngineBuilderInfo(CronetEngineBuilderImpl builder) {
            mPublicKeyPinningBypassForLocalTrustAnchorsEnabled =
                    builder.publicKeyPinningBypassForLocalTrustAnchorsEnabled();
            mUserAgent = builder.getUserAgent();
            mStoragePath = builder.storagePath();
            mQuicEnabled = builder.quicEnabled();
            mHttp2Enabled = builder.http2Enabled();
            mBrotiEnabled = builder.brotliEnabled();
            mHttpCacheMode = builder.publicBuilderHttpCacheMode();
            mExperimentalOptions = builder.experimentalOptions();
            mNetworkQualityEstimatorEnabled = builder.networkQualityEstimatorEnabled();
            mThreadPriority = builder.threadPriority(THREAD_PRIORITY_BACKGROUND);
        }

        /**
         * @return Whether public key pinning bypass for local trust anchors is enabled
         */
        public boolean isPublicKeyPinningBypassForLocalTrustAnchorsEnabled() {
            return mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
        }
        /**
         * @return User-Agent used for URLRequests created through this CronetEngine
         */
        public String getUserAgent() {
            return mUserAgent;
        }
        /**
         * @return Path to the directory used for HTTP cache and Cookie storage
         */
        public String getStoragePath() {
            return mStoragePath;
        }

        /**
         * @return Whether QUIC protocol is enabled
         */
        public boolean isQuicEnabled() {
            return mQuicEnabled;
        }

        /**
         * @return Whether HTTP2 protocol is enabled
         */
        public boolean isHttp2Enabled() {
            return mHttp2Enabled;
        }

        /**
         * @return Whether Brotli compression is enabled
         */
        public boolean isBrotliEnabled() {
            return mBrotiEnabled;
        }

        /**
         * @return Whether caching of HTTP data and other information like QUIC server information
         *         is enabled
         */
        public int getHttpCacheMode() {
            return mHttpCacheMode;
        }

        /**
         * @return Experimental options configuration used by the CronetEngine
         */
        public String getExperimentalOptions() {
            return mExperimentalOptions;
        }

        /**
         * @return Whether network quality estimator is enabled
         */
        public boolean isNetworkQualityEstimatorEnabled() {
            return mNetworkQualityEstimatorEnabled;
        }

        /**
         * @return The thread priority of Cronet's internal thread
         */
        public int getThreadPriority() {
            return mThreadPriority;
        }
    }

    /**
     * Aggregates the information about request and response traffic for a
     * particular CronetEngine.
     */
    public static class CronetTrafficInfo {
        private final long mRequestHeaderSizeInBytes;
        private final long mRequestBodySizeInBytes;
        private final long mResponseHeaderSizeInBytes;
        private final long mResponseBodySizeInBytes;
        private final int mResponseStatusCode;
        private final Duration mHeadersLatency;
        private final Duration mTotalLatency;
        private final String mNegotiatedProtocol;
        private final boolean mWasConnectionMigrationAttempted;
        private final boolean mDidConnectionMigrationSucceed;

        public CronetTrafficInfo(long requestHeaderSizeInBytes, long requestBodySizeInBytes,
                long responseHeaderSizeInBytes, long responseBodySizeInBytes,
                int responseStatusCode, Duration headersLatency, Duration totalLatency,
                String negotiatedProtocol, boolean wasConnectionMigrationAttempted,
                boolean didConnectionMigrationSucceed) {
            mRequestHeaderSizeInBytes = requestHeaderSizeInBytes;
            mRequestBodySizeInBytes = requestBodySizeInBytes;
            mResponseHeaderSizeInBytes = responseHeaderSizeInBytes;
            mResponseBodySizeInBytes = responseBodySizeInBytes;
            mResponseStatusCode = responseStatusCode;
            mHeadersLatency = headersLatency;
            mTotalLatency = totalLatency;
            mNegotiatedProtocol = negotiatedProtocol;
            mWasConnectionMigrationAttempted = wasConnectionMigrationAttempted;
            mDidConnectionMigrationSucceed = didConnectionMigrationSucceed;
        }

        /**
         * @return The total size of headers sent in bytes
         */
        public long getRequestHeaderSizeInBytes() {
            return mRequestHeaderSizeInBytes;
        }

        /**
         * @return The total size of request body sent, if any, in bytes
         */
        public long getRequestBodySizeInBytes() {
            return mRequestBodySizeInBytes;
        }

        /**
         * @return The total size of headers received in bytes
         */
        public long getResponseHeaderSizeInBytes() {
            return mResponseHeaderSizeInBytes;
        }

        /**
         * @return The total size of response body, if any, received in bytes
         */
        public long getResponseBodySizeInBytes() {
            return mResponseBodySizeInBytes;
        }

        /**
         * @return The response status code of the request
         */
        public int getResponseStatusCode() {
            return mResponseStatusCode;
        }

        /**
         * The time it took from starting the request to receiving the full set of
         * response headers.
         *
         * @return The time to get response headers
         */
        public Duration getHeadersLatency() {
            return mHeadersLatency;
        }

        /**
         * The time it took from starting the request to receiving the entire
         * response.
         *
         * @return The time to get total response
         */
        public Duration getTotalLatency() {
            return mTotalLatency;
        }

        /**
         * @return The negotiated protocol used for the traffic
         */
        public String getNegotiatedProtocol() {
            return mNegotiatedProtocol;
        }

        /**
         * @return True if the connection migration was attempted, else False
         */
        public boolean wasConnectionMigrationAttempted() {
            return mWasConnectionMigrationAttempted;
        }

        /**
         * @return True if the connection migration was attempted and succeeded, else False
         */
        public boolean didConnectionMigrationSucceed() {
            return mDidConnectionMigrationSucceed;
        }
    }

    /**
     * Holds information about the cronet version used for a cronetEngine.
     */
    public static class CronetVersion {
        private final int mMajorVersion;
        private final int mMinorVersion;
        private final int mBuildVersion;
        private final int mPatchVersion;

        /**
         * Pass the cronet version string here and
         * it would be split. The string comes in the format
         * MAJOR.MINOR.BUILD.PATCH
         */
        public CronetVersion(String version) {
            String[] splitVersion = version.split("\\.");
            mMajorVersion = Integer.parseInt(splitVersion[0]);
            mMinorVersion = Integer.parseInt(splitVersion[1]);
            mBuildVersion = Integer.parseInt(splitVersion[2]);
            mPatchVersion = Integer.parseInt(splitVersion[3]);
        }

        /**
         * @return the MAJOR version of cronet used for the traffic
         */
        public int getMajorVersion() {
            return mMajorVersion;
        }

        /**
         * @return the MINOR version of cronet used for the traffic
         */
        public int getMinorVersion() {
            return mMinorVersion;
        }

        /**
         * @return the BUILD version of cronet used for the traffic
         */
        public int getBuildVersion() {
            return mBuildVersion;
        }

        /**
         * @return the PATCH version of cronet used for the traffic
         */
        public int getPatchVersion() {
            return mPatchVersion;
        }

        @Override
        public String toString() {
            return "" + mMajorVersion + "." + mMinorVersion + "." + mBuildVersion + "."
                    + mPatchVersion;
        }
    }
}
