// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.sync.protocol.GroupData;

/** Config class for the Data Sharing Create UI. */
public class DataSharingCreateUiConfig {

    // --- Create Usage Config ---
    private Bitmap mPreviewImage;
    private CreateCallback mCreateCallback;
    private DataSharingUiConfig mCommonConfig;

    /** Callback interface for data sharing create UI events. */
    public interface CreateCallback {
        default void onGroupCreated(GroupData groupData) {}

        default void onCancelClicked() {}

        default void getDataSharingUrl(GroupToken tokenSecret, Callback<String> url) {}
    }

    private DataSharingCreateUiConfig(Builder builder) {
        this.mPreviewImage = builder.mPreviewImage;
        this.mCreateCallback = builder.mCreateCallback;
        this.mCommonConfig = builder.mCommonConfig;
    }

    public Bitmap getPreviewImage() {
        return mPreviewImage;
    }

    public CreateCallback getCreateCallback() {
        return mCreateCallback;
    }

    public DataSharingUiConfig getCommonConfig() {
        return mCommonConfig;
    }

    // Builder class
    public static class Builder {
        private Bitmap mPreviewImage;
        private CreateCallback mCreateCallback;
        private DataSharingUiConfig mCommonConfig;

        /**
         * Sets the preview image for the tab group.
         *
         * @param previewImage The preview image to display.
         */
        public Builder setPreviewImage(Bitmap previewImage) {
            this.mPreviewImage = previewImage;
            return this;
        }

        /**
         * Sets the callback for create UI events.
         *
         * @param createCallback The callback to use for create events.
         */
        public Builder setCreateCallback(CreateCallback createCallback) {
            this.mCreateCallback = createCallback;
            return this;
        }

        /**
         * Sets the data sharing common UI config.
         *
         * @param commonConfig The common UI configuration.
         */
        public Builder setCommonConfig(DataSharingUiConfig commonConfig) {
            this.mCommonConfig = commonConfig;
            return this;
        }

        public DataSharingCreateUiConfig build() {
            return new DataSharingCreateUiConfig(this);
        }
    }
}
