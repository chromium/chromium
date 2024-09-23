// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.ConnectionCloseSource;

import java.time.Duration;
import java.util.List;

/** Base class for implementing a CronetLogger. */
public abstract class CronetLogger {
    // TODO(b/313418339): align the naming with the atom definition.
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
        // The application is using the fake implementation.
        CRONET_SOURCE_FAKE,
    }

    /** Generates a new unique ID suitable for use as reference for cross-linking log events. */
    public abstract long generateId();

    public abstract void logCronetEngineBuilderInitializedInfo(
            CronetEngineBuilderInitializedInfo info);

    public abstract void logCronetInitializedInfo(CronetInitializedInfo info);

    /**
     * Logs a cronetEngine creation action with the details of the creation.
     *
     * @param cronetEngineId the id of the engine being created.
     * @param engineBuilderInfo the configuration of the CronetEngine being created. See {@link
     *     CronetEngineBuilderInfo}
     * @param version the version of cronet used for the engine. See {@link CronetVersion}
     * @param source the source of the cronet provider for the engine. See {@link CronetSource}
     */
    public abstract void logCronetEngineCreation(
            long cronetEngineId,
            CronetEngineBuilderInfo engineBuilderInfo,
            CronetVersion version,
            CronetSource source);

    /**
     * Logs a request/response action.
     *
     * @param cronetEngineId the id of the engine used for the request
     * @param trafficInfo the associated traffic information. See {@link CronetTrafficInfo}
     */
    public abstract void logCronetTrafficInfo(long cronetEngineId, CronetTrafficInfo trafficInfo);

    // TODO(crbug.com/41494309): consider using AutoValue for this.
    public static final class CronetEngineBuilderInitializedInfo {
        public long cronetInitializationRef;

        public static enum Author {
            API,
            IMPL
        }

        public Author author;
        public int engineBuilderCreatedLatencyMillis = -1;
        public CronetSource source = CronetSource.CRONET_SOURCE_UNSPECIFIED;
        public Boolean creationSuccessful;
        public CronetVersion apiVersion;
        public CronetVersion implVersion;
        public int uid;
    }

    public static final class CronetInitializedInfo {
        public long cronetInitializationRef;
        public int engineCreationLatencyMillis = -1;
        public int engineAsyncLatencyMillis = -1;
        public int httpFlagsLatencyMillis = -1;
        public Boolean httpFlagsSuccessful;
        public List<Long> httpFlagsNames;
        public List<Long> httpFlagsValues;
    }

    /** Aggregates the information about a CronetEngine configuration. */
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
        private final long mCronetInitializationRef;

        public CronetEngineBuilderInfo(
                boolean publicKeyPinningBypassForLocalTrustAnchorsEnabled,
                String userAgent,
                String storagePath,
                boolean quicEnabled,
                boolean http2Enabled,
                boolean brotiEnabled,
                int httpCacheMode,
                String experimentalOptions,
                boolean networkQualityEstimatorEnabled,
                int threadPriority,
                long cronetInitializationRef) {
            mPublicKeyPinningBypassForLocalTrustAnchorsEnabled =
                    publicKeyPinningBypassForLocalTrustAnchorsEnabled;
            mUserAgent = userAgent;
            mStoragePath = storagePath;
            mQuicEnabled = quicEnabled;
            mHttp2Enabled = http2Enabled;
            mBrotiEnabled = brotiEnabled;
            mHttpCacheMode = httpCacheMode;
            mExperimentalOptions = experimentalOptions;
            mNetworkQualityEstimatorEnabled = networkQualityEstimatorEnabled;
            mThreadPriority = threadPriority;
            mCronetInitializationRef = cronetInitializationRef;
        }

        /** @return Whether public key pinning bypass for local trust anchors is enabled */
        public boolean isPublicKeyPinningBypassForLocalTrustAnchorsEnabled() {
            return mPublicKeyPinningBypassForLocalTrustAnchorsEnabled;
        }

        /** @return User-Agent used for URLRequests created through this CronetEngine */
        public String getUserAgent() {
            return mUserAgent;
        }

        /** @return Path to the directory used for HTTP cache and Cookie storage */
        public String getStoragePath() {
            return mStoragePath;
        }

        /** @return Whether QUIC protocol is enabled */
        public boolean isQuicEnabled() {
            return mQuicEnabled;
        }

        /** @return Whether HTTP2 protocol is enabled */
        public boolean isHttp2Enabled() {
            return mHttp2Enabled;
        }

        /** @return Whether Brotli compression is enabled */
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

        /** @return Experimental options configuration used by the CronetEngine */
        public String getExperimentalOptions() {
            return mExperimentalOptions;
        }

        /** @return Whether network quality estimator is enabled */
        public boolean isNetworkQualityEstimatorEnabled() {
            return mNetworkQualityEstimatorEnabled;
        }

        /** @return The thread priority of Cronet's internal thread */
        public int getThreadPriority() {
            return mThreadPriority;
        }

        public long getCronetInitializationRef() {
            return mCronetInitializationRef;
        }
    }

    /**
     * Aggregates the information about request and response traffic for a
     * particular CronetEngine.
     */
    public static class CronetTrafficInfo {
        public static enum RequestTerminalState {
            SUCCEEDED,
            ERROR,
            CANCELLED,
        }

        // TODO(b/355615357): Add more specific failure reasons.
        public static enum RequestFailureReason {
            UNKNOWN,
            NETWORK,
            OTHER,
        }

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
        private final RequestTerminalState mTerminalState;
        private final int mNonfinalUserCallbackExceptionCount;
        private final int mReadCount;
        private final int mOnUploadReadCount;
        private final boolean mIsBidiStream;
        private final boolean mFinalUserCallbackThrew;
        private final int mUid;
        private final int mNetworkInternalErrorCode;
        private final int mQuicErrorCode;
        private final @ConnectionCloseSource int mSource;
        private final RequestFailureReason mFailureReason;
        private final boolean mSocketReused;

        public CronetTrafficInfo(
                long requestHeaderSizeInBytes,
                long requestBodySizeInBytes,
                long responseHeaderSizeInBytes,
                long responseBodySizeInBytes,
                int responseStatusCode,
                Duration headersLatency,
                Duration totalLatency,
                String negotiatedProtocol,
                boolean wasConnectionMigrationAttempted,
                boolean didConnectionMigrationSucceed,
                RequestTerminalState terminalState,
                int nonfinalUserCallbackExceptionCount,
                int readCount,
                int uploadReadCount,
                boolean isBidiStream,
                boolean finalUserCallbackThrew,
                int uid,
                int networkInternalErrorCode,
                int quicErrorCode,
                @ConnectionCloseSource int source,
                RequestFailureReason failureReason,
                boolean sockedReused) {
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
            mTerminalState = terminalState;
            mNonfinalUserCallbackExceptionCount = nonfinalUserCallbackExceptionCount;
            mReadCount = readCount;
            mOnUploadReadCount = uploadReadCount;
            mIsBidiStream = isBidiStream;
            mFinalUserCallbackThrew = finalUserCallbackThrew;
            mUid = uid;
            mNetworkInternalErrorCode = networkInternalErrorCode;
            mQuicErrorCode = quicErrorCode;
            mSource = source;
            mFailureReason = failureReason;
            mSocketReused = sockedReused;
        }

        /**
         * @return The total size of headers sent in bytes
         */
        public long getRequestHeaderSizeInBytes() {
            return mRequestHeaderSizeInBytes;
        }

        /** @return The total size of request body sent, if any, in bytes */
        public long getRequestBodySizeInBytes() {
            return mRequestBodySizeInBytes;
        }

        /** @return The total size of headers received in bytes */
        public long getResponseHeaderSizeInBytes() {
            return mResponseHeaderSizeInBytes;
        }

        /** @return The total size of response body, if any, received in bytes */
        public long getResponseBodySizeInBytes() {
            return mResponseBodySizeInBytes;
        }

        /** @return The response status code of the request */
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

        /** @return The negotiated protocol used for the traffic */
        public String getNegotiatedProtocol() {
            return mNegotiatedProtocol;
        }

        /** @return True if the connection migration was attempted, else False */
        public boolean wasConnectionMigrationAttempted() {
            return mWasConnectionMigrationAttempted;
        }

        /** @return True if the connection migration was attempted and succeeded, else False */
        public boolean didConnectionMigrationSucceed() {
            return mDidConnectionMigrationSucceed;
        }

        public RequestTerminalState getTerminalState() {
            return mTerminalState;
        }

        public int getNonfinalUserCallbackExceptionCount() {
            return mNonfinalUserCallbackExceptionCount;
        }

        public int getReadCount() {
            return mReadCount;
        }

        public int getOnUploadReadCount() {
            return mOnUploadReadCount;
        }

        public boolean getIsBidiStream() {
            return mIsBidiStream;
        }

        public boolean getFinalUserCallbackThrew() {
            return mFinalUserCallbackThrew;
        }

        public int getUid() {
            return mUid;
        }

        public int getNetworkInternalErrorCode() {
            return mNetworkInternalErrorCode;
        }

        public int getQuicErrorCode() {
            return mQuicErrorCode;
        }

        public @ConnectionCloseSource int getConnectionCloseSource() {
            return mSource;
        }

        public RequestFailureReason getFailureReason() {
            return mFailureReason;
        }

        public boolean getIsSocketReused() {
            return mSocketReused;
        }
    }

    /** Holds information about the cronet version used for a cronetEngine. */
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

        /** @return the MAJOR version of cronet used for the traffic */
        public int getMajorVersion() {
            return mMajorVersion;
        }

        /** @return the MINOR version of cronet used for the traffic */
        public int getMinorVersion() {
            return mMinorVersion;
        }

        /** @return the BUILD version of cronet used for the traffic */
        public int getBuildVersion() {
            return mBuildVersion;
        }

        /** @return the PATCH version of cronet used for the traffic */
        public int getPatchVersion() {
            return mPatchVersion;
        }

        @Override
        public String toString() {
            return ""
                    + mMajorVersion
                    + "."
                    + mMinorVersion
                    + "."
                    + mBuildVersion
                    + "."
                    + mPatchVersion;
        }
    }
}
