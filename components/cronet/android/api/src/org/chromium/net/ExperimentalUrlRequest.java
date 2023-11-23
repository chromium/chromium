// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import java.util.concurrent.Executor;

/**
 * {@link UrlRequest} that exposes experimental features. To obtain an instance of this class, cast
 * a {@code UrlRequest} to this type. Every instance of {@code UrlRequest} can be cast to an
 * instance of this class, as they are backed by the same implementation and hence perform
 * identically. Instances of this class are not meant for general use, but instead only to access
 * experimental features. Experimental features may be deprecated in the future. Use at your own
 * risk.
 *
 * {@hide for consistency with other experimental classes}
 *
 * @deprecated scheduled for deletion, don't use in new code.
 */
@Deprecated
public abstract class ExperimentalUrlRequest extends UrlRequest {
    /**
     * {@link UrlRequest#Builder} that exposes experimental features. To obtain an instance of this
     * class, cast a {@code UrlRequest.Builder} to this type. Every instance of {@code
     * UrlRequest.Builder} can be cast to an instance of this class, as they are backed by the same
     * implementation and hence perform identically. Instances of this class are not meant for
     * general use, but instead only to access experimental features. Experimental features may be
     * deprecated in the future. Use at your own risk.
     *
     * {@hide for consistency with other experimental classes}
     *
     * @deprecated scheduled for deletion, don't use in new code.
     */
    @Deprecated
    public abstract static class Builder extends UrlRequest.Builder {
        /**
         * Disables connection migration for the request if enabled for the session.
         *
         * @return the builder to facilitate chaining.
         */
        public Builder disableConnectionMigration() {
            return this;
        }

        /**
         * Default request idempotency, only enable 0-RTT for safe HTTP methods. Passed to {@link
         * #setIdempotency}.
         */
        public static final int DEFAULT_IDEMPOTENCY = 0;

        /** Request is idempotent. Passed to {@link #setIdempotency}. */
        public static final int IDEMPOTENT = 1;

        /** Request is not idempotent. Passed to {@link #setIdempotency}. */
        public static final int NOT_IDEMPOTENT = 2;

        /**
         * Sets idempotency of the request which should be one of the {@link #DEFAULT_IDEMPOTENCY
         * IDEMPOTENT NOT_IDEMPOTENT} values. The default idempotency indicates that 0-RTT is only
         * enabled for safe HTTP methods (GET, HEAD, OPTIONS, and TRACE).
         *
         * @param idempotency idempotency of the request which should be one of the {@link
         * #DEFAULT_IDEMPOTENCY IDEMPOTENT NOT_IDEMPOTENT} values.
         * @return the builder to facilitate chaining.
         */
        public Builder setIdempotency(int idempotency) {
            return this;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public abstract Builder setHttpMethod(String method);

        @Override
        public abstract Builder addHeader(String header, String value);

        @Override
        public abstract Builder disableCache();

        @Override
        public abstract Builder setPriority(int priority);

        @Override
        public abstract Builder setUploadDataProvider(
                UploadDataProvider uploadDataProvider, Executor executor);

        @Override
        public abstract Builder allowDirectExecutor();

        @Override
        public abstract ExperimentalUrlRequest build();

        @Override
        public Builder addRequestAnnotation(Object annotation) {
            return this;
        }

        @Override
        public Builder setTrafficStatsTag(int tag) {
            return this;
        }

        @Override
        public Builder setTrafficStatsUid(int uid) {
            return this;
        }

        @Override
        public Builder setRequestFinishedListener(RequestFinishedInfo.Listener listener) {
            return this;
        }
    }
}
