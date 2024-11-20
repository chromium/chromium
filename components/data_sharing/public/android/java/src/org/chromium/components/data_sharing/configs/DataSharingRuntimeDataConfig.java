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

    public DataSharingRuntimeDataConfig(
            String sessionId, DataSharingPreviewDataConfig dataSharingPreviewDataConfig) {
        this.mSessionId = sessionId;
        this.mDataSharingPreviewDataConfig = dataSharingPreviewDataConfig;
    }

    public String getSessionId() {
        return mSessionId;
    }

    public DataSharingPreviewDataConfig getDataSharingPreviewDataConfig() {
        return mDataSharingPreviewDataConfig;
    }

    // Builder class
    public static class Builder {
        private String mSessionId;
        private DataSharingPreviewDataConfig mDataSharingPreviewDataConfig;

        /** Session ID to set the preview to, given when showXFlow API is called */
        public Builder setSessionId(String sessionId) {
            this.mSessionId = sessionId;
            return this;
        }

        /** Preview data data for the flows */
        public Builder setDataSharingPreviewDataConfig(
                DataSharingPreviewDataConfig dataSharingPreviewDataConfig) {
            this.mDataSharingPreviewDataConfig = dataSharingPreviewDataConfig;
            return this;
        }

        public DataSharingRuntimeDataConfig build() {
            return new DataSharingRuntimeDataConfig(mSessionId, mDataSharingPreviewDataConfig);
        }
    }
}
