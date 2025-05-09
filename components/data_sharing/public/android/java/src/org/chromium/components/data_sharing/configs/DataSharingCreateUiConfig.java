// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.sync.protocol.CollaborationGroupMetadata;
import org.chromium.components.sync.protocol.GroupData;

/** Config class for the Data Sharing Create UI. */
@NullMarked
public class DataSharingCreateUiConfig {

    // --- Create Usage Config ---
    private final @Nullable Bitmap mPreviewImage;
    private final @Nullable CollaborationGroupMetadata mCollaborationGroupMetadata;
    private final @Nullable CreateCallback mCreateCallback;
    private final @Nullable DataSharingUiConfig mCommonConfig;

    /** Callback interface for data sharing create UI events. */
    public interface CreateCallback {
        default void onGroupCreated(GroupData groupData) {}

        default void onGroupCreatedWithWait(
                GroupData groupData, Callback<Boolean> onCreateFinished) {}

        default void onCancelClicked() {}

        default void getDataSharingUrl(GroupToken tokenSecret, Callback<String> url) {}

        // This will always be called when user creates the group, ui closes, or
        // session is finished.
        default void onSessionFinished() {}
    }

    private DataSharingCreateUiConfig(Builder builder) {
        this.mPreviewImage = builder.mPreviewImage;
        this.mCollaborationGroupMetadata = builder.mCollaborationGroupMetadata;
        this.mCreateCallback = builder.mCreateCallback;
        this.mCommonConfig = builder.mCommonConfig;
    }

    public @Nullable Bitmap getPreviewImage() {
        return mPreviewImage;
    }

    public @Nullable CollaborationGroupMetadata getCollaborationGroupMetadata() {
        return mCollaborationGroupMetadata;
    }

    public @Nullable CreateCallback getCreateCallback() {
        return mCreateCallback;
    }

    public @Nullable DataSharingUiConfig getCommonConfig() {
        return mCommonConfig;
    }

    // Builder class
    public static class Builder {
        private @Nullable Bitmap mPreviewImage;
        private @Nullable CollaborationGroupMetadata mCollaborationGroupMetadata;
        private @Nullable CreateCallback mCreateCallback;
        private @Nullable DataSharingUiConfig mCommonConfig;

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
         * Sets the metadata of the group.
         *
         * @param collaborationGroupMetadata The metadata of the group.
         */
        public Builder setCollaborationGroupMetadata(CollaborationGroupMetadata collaborationGroupMetadata) {
            this.mCollaborationGroupMetadata = collaborationGroupMetadata;
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
