// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.sync.protocol.GroupData;
import org.chromium.components.sync.protocol.GroupMember;

/** Config class for the Data Sharing Manage UI. */
public class DataSharingManageUiConfig {

    // --- Group related Info ---
    private GroupToken mGroupToken;

    // --- Manage Usage Config ---
    private ManageCallback mManageCallback;
    private DataSharingUiConfig mCommonConfig;

    /** Callback interface for data sharing Manage UI events. */
    public interface ManageCallback {
        default void onLinkSharingToggled(boolean isLinkSharingEnabled, GroupData groupData) {}

        default void onStopSharingInitiated(GroupData groupData, Callback<Boolean> readyToStop) {}

        default void onStopSharingSuccess(GroupData groupData) {}

        default void onMemberRemoved(GroupMember member) {}

        default void onMemberBlocked(GroupMember member) {}

        default void onMemberRemovedAndStopSharingInitiated(
                GroupMember member, GroupData groupData, Callback<Boolean> readyToStop) {}

        default void onMemberBlockedAndLeaveGroup(GroupMember member, GroupData groupData) {}

        default void onLeaveGroup() {}

        default void getDataSharingUrl(GroupToken groupToken, Callback<String> url) {}
    }

    private DataSharingManageUiConfig(Builder builder) {
        this.mGroupToken = builder.mGroupToken;
        this.mManageCallback = builder.mManageCallback;
        this.mCommonConfig = builder.mCommonConfig;
    }

    public GroupToken getGroupToken() {
        return mGroupToken;
    }

    public ManageCallback getManageCallback() {
        return mManageCallback;
    }

    public DataSharingUiConfig getCommonConfig() {
        return mCommonConfig;
    }

    // Builder class
    public static class Builder {
        private GroupToken mGroupToken;
        private ManageCallback mManageCallback;
        private DataSharingUiConfig mCommonConfig;

        /**
         * Sets the group token for the data sharing group.
         *
         * @param groupToken The token for the data sharing group includes groupId and token.
         */
        public Builder setGroupToken(GroupToken groupToken) {
            this.mGroupToken = groupToken;
            return this;
        }

        /**
         * Sets the callback for manage UI events.
         *
         * @param manageCallback The callback to use for managing UI events.
         */
        public Builder setManageCallback(ManageCallback manageCallback) {
            this.mManageCallback = manageCallback;
            return this;
        }

        /**
         * Sets the data sharing common UI config.
         *
         * @param commonConfig The common UI config.
         */
        public Builder setCommonConfig(DataSharingUiConfig commonConfig) {
            this.mCommonConfig = commonConfig;
            return this;
        }

        public DataSharingManageUiConfig build() {
            return new DataSharingManageUiConfig(this);
        }
    }
}
