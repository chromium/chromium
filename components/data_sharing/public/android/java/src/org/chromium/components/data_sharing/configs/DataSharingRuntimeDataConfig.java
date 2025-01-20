// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

/**
 * Holds runtime information needed for updating flows after they started.
 *
 * <p>Only one type of data can be set at once.
 */
public class DataSharingRuntimeDataConfig {
    private final String mSessionId;
    private final DataSharingPreviewDataConfig mDataSharingPreviewDataConfig;
    private final DataSharingPreviewDetailsConfig mDataSharingPreviewDetailsConfig;

    public DataSharingRuntimeDataConfig(
            String sessionId, DataSharingPreviewDataConfig dataSharingPreviewDataConfig) {
        this.mSessionId = sessionId;
        this.mDataSharingPreviewDataConfig = dataSharingPreviewDataConfig;
        this.mDataSharingPreviewDetailsConfig = null;
    }

    public DataSharingRuntimeDataConfig(
            String sessionId, DataSharingPreviewDetailsConfig dataSharingPreviewDetailsConfig) {
        this.mSessionId = sessionId;
        this.mDataSharingPreviewDataConfig = null;
        this.mDataSharingPreviewDetailsConfig = dataSharingPreviewDetailsConfig;
    }

    public String getSessionId() {
        return mSessionId;
    }

    public DataSharingPreviewDataConfig getDataSharingPreviewDataConfig() {
        return mDataSharingPreviewDataConfig;
    }

    public DataSharingPreviewDetailsConfig getDataSharingPreviewDetailsConfig() {
        return mDataSharingPreviewDetailsConfig;
    }

    // Builder class
    public static class Builder {
        private String mSessionId;
        private DataSharingPreviewDataConfig mDataSharingPreviewDataConfig;
        private DataSharingPreviewDetailsConfig mDataSharingPreviewDetailsConfig;
        private boolean mRuntimeDataSet;

        /** Session ID to set the preview to, given when showXFlow API is called */
        public Builder setSessionId(String sessionId) {
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

        public DataSharingRuntimeDataConfig build() {
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
