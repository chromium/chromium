// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.ExperimentalCronetEngine;

/**
 * Base class for implementing a CronetLogger.
 */
public abstract class CronetLogger {
    public static enum CronetSource {
        // Safe default, don't use explicitly.
        CRONET_SOURCE_UNSPECIFIED,
        // The library is bundled with the application.
        CRONET_SOURCE_STATICALLY_LINKED,
        // The library is loaded from GooglePlayServices
        CRONET_SOURCE_PLAY_SERVICES,
        // The application is using the fallback implementation
        CRONET_SOURCE_FALLBACK,
    }

    public abstract void logCronetEngineCreation(int cronetEngineId,
            ExperimentalCronetEngine.Builder builder, CronetVersion version, CronetSource source);

    public abstract void logCronetTrafficInfo(int cronetEngineId, CronetTrafficInfo trafficInfo);

    /**
     * This aggregates the information about request and response traffic for a
     * particular CronetEngine
     */
    public static class CronetTrafficInfo {
        private final int mRequestHeaderSizeInBytes;
        private final int mRequestBodySizeInBytes;
        private final int mResponseHeaderSizeInBytes;
        private final int mResponseBodySizeInBytes;
        private final int mResponseStatusCode;
        private final int mHeadersLatencyInMillis;
        private final int mTotalLatencyInMillis;
        private final String mNegotiatedProtocol;
        private final boolean mWasConnectionMigrationAttempted;
        private final boolean mDidConnectionMigrationSucceed;

        public CronetTrafficInfo(int requestHeaderSizeInBytes, int requestBodySizeInBytes,
                int responseHeaderSizeInBytes, int responseBodySizeInBytes, int responseStatusCode,
                int headersLatencyInMillis, int totalLatencyInMillis, String negotiatedProtocol,
                boolean wasConnectionMigrationAttempted, boolean didConnectionMigrationSucceed) {
            mRequestHeaderSizeInBytes = requestHeaderSizeInBytes;
            mRequestBodySizeInBytes = requestBodySizeInBytes;
            mResponseHeaderSizeInBytes = responseHeaderSizeInBytes;
            mResponseBodySizeInBytes = responseBodySizeInBytes;
            mResponseStatusCode = responseStatusCode;
            mHeadersLatencyInMillis = headersLatencyInMillis;
            mTotalLatencyInMillis = totalLatencyInMillis;
            mNegotiatedProtocol = negotiatedProtocol;
            mWasConnectionMigrationAttempted = wasConnectionMigrationAttempted;
            mDidConnectionMigrationSucceed = didConnectionMigrationSucceed;
        }

        /**
         * @return The total size of headers sent in bytes
         */
        public int getRequestHeaderSizeInBytes() {
            return mRequestHeaderSizeInBytes;
        }

        /**
         * @return The total size of request body sent, if any, in bytes
         */
        public int getRequestBodySizeInBytes() {
            return mRequestBodySizeInBytes;
        }

        /**
         * @return The total size of headers received in bytes
         */
        public int getResponseHeaderSizeInBytes() {
            return mResponseHeaderSizeInBytes;
        }

        /**
         * @return The total size of response body, if any, received in bytes
         */
        public int getResponseBodySizeInBytes() {
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
         * @return The time to get response headers in milliseconds
         */
        public int getHeadersLatencyInMillis() {
            return mHeadersLatencyInMillis;
        }

        /**
         * The time it took from starting the request to receiving the entire
         * response.
         *
         * @return The time to get total response in milliseconds
         */
        public int getTotalLatencyInMillis() {
            return mTotalLatencyInMillis;
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
    }
}