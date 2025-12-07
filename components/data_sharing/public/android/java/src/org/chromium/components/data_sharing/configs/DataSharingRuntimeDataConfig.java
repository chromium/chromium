// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Holds runtime information needed for updating flows after they started.
 *
 * <p>Only one type of data can be set at once.
 */
@NullMarked
public class DataSharingRuntimeDataConfig {
    private final @Nullable String mSessionId;
    private final @Nullable DataSharingPreviewDataConfig mDataSharingPreviewDataConfig;
    private final @Nullable DataSharingPreviewDetailsConfig mDataSharingPreviewDetailsConfig;

    public DataSharingRuntimeDataConfig(
            @Nullable String sessionId, DataSharingPreviewDataConfig dataSharingPreviewDataConfig) {
        this.mSessionId = sessionId;
        this.mDataSharingPreviewDataConfig = dataSharingPreviewDataConfig;
        this.mDataSharingPreviewDetailsConfig = null;
    }

    public DataSharingRuntimeDataConfig(
            @Nullable String sessionId,
            DataSharingPreviewDetailsConfig dataSharingPreviewDetailsConfig) {
        this.mSessionId = sessionId;
        this.mDataSharingPreviewDataConfig = null;
        this.mDataSharingPreviewDetailsConfig = dataSharingPreviewDetailsConfig;
    }

    public @Nullable String getSessionId() {
        return mSessionId;
    }

    public @Nullable DataSharingPreviewDataConfig getDataSharingPreviewDataConfig() {
        return mDataSharingPreviewDataConfig;
    }

    public @Nullable DataSharingPreviewDetailsConfig getDataSharingPreviewDetailsConfig() {
        return mDataSharingPreviewDetailsConfig;
    }

    // Builder class
    public static class Builder {
        private @Nullable String mSessionId;
        private @Nullable DataSharingPreviewDataConfig mDataSharingPreviewDataConfig;
        private @Nullable DataSharingPreviewDetailsConfig mDataSharingPreviewDetailsConfig;
        private boolean mRuntimeDataSet;

        /** Session ID to set the preview to, given when showXFlow API is called */
        public Builder setSessionId(@Nullable String sessionId) {
            this.mSessionId = sessionId;
            return this;
        }

        /** Preview image data for the flows */
        public Builder setDataSharingPreviewDataConfig(
                DataSharingPreviewDataConfig dataSharingPreviewDataConfig) {
            assert !mRuntimeDataSet;
            mRuntimeDataSet = true; // Only one is allowed.
            this.mDataSharingPreviewDataConfig = dataSharingPreviewDataConfig;
            return this;
        }

        /** Preview details for each tab in tab group */
        public Builder setDataSharingPreviewDetailsConfig(
                DataSharingPreviewDetailsConfig dataSharingPreviewDetailsConfig) {
            assert !mRuntimeDataSet;
            mRuntimeDataSet = true; // Only one is allowed.
            this.mDataSharingPreviewDetailsConfig = dataSharingPreviewDetailsConfig;
            return this;
        }

        public @Nullable DataSharingRuntimeDataConfig build() {
            if (mDataSharingPreviewDataConfig != null) {
                return new DataSharingRuntimeDataConfig(mSessionId, mDataSharingPreviewDataConfig);
            } else if (mDataSharingPreviewDetailsConfig != null) {
                return new DataSharingRuntimeDataConfig(
                        mSessionId, mDataSharingPreviewDetailsConfig);
            }
            return null;
        }
    }
}
