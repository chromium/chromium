// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.sync.protocol.GroupData;
import org.chromium.components.sync.protocol.GroupMember;
import org.chromium.url.GURL;

/** Config class for the Data Sharing Manage UI. */
@NullMarked
public class DataSharingManageUiConfig {

    // --- Group related Info ---
    private final @Nullable GroupToken mGroupToken;

    // --- Manage Usage Config ---
    private final @Nullable ManageCallback mManageCallback;
    private final @Nullable GURL mLearnAboutBlockedAccounts;
    private final @Nullable GURL mActivityLogsUrl;
    private final @Nullable DataSharingUiConfig mCommonConfig;
    private final boolean mIsSharingDisabled;

    /** Callback interface for data sharing Manage UI events. */
    public interface ManageCallback {
        default void onLinkSharingToggled(boolean isLinkSharingEnabled, GroupData groupData) {}

        default void onShareInviteLinkClicked(GroupToken groupToken) {}

        default void onShareInviteLinkClickedWithWait(
                GroupToken groupToken, Callback<Boolean> onShareDone) {}

        default void onStopSharingInitiated(Callback<Boolean> readyToStopSharing) {}

        default void onStopSharingCompleted(boolean success) {}

        default void onMemberRemoved(GroupMember member) {}

        default void onMemberBlocked(GroupMember member) {}

        default void onLeaveGroup() {}

        default void getDataSharingUrl(GroupToken groupToken, Callback<String> url) {}

        // This will always be called when user exits the managing the group,
        // ui closes, or session is finished.
        default void onSessionFinished() {}
    }

    private DataSharingManageUiConfig(Builder builder) {
        this.mGroupToken = builder.mGroupToken;
        this.mLearnAboutBlockedAccounts = builder.mLearnAboutBlockedAccounts;
        this.mActivityLogsUrl = builder.mActivityLogsUrl;
        this.mManageCallback = builder.mManageCallback;
        this.mCommonConfig = builder.mCommonConfig;
        this.mIsSharingDisabled = builder.mIsSharingDisabled;
    }

    public @Nullable GroupToken getGroupToken() {
        return mGroupToken;
    }

    public @Nullable ManageCallback getManageCallback() {
        return mManageCallback;
    }

    public @Nullable GURL getLearnAboutBlockedAccounts() {
        return mLearnAboutBlockedAccounts;
    }

    public @Nullable GURL getActivityLogsUrl() {
        return mActivityLogsUrl;
    }

    public @Nullable DataSharingUiConfig getCommonConfig() {
        return mCommonConfig;
    }

    public boolean getIsSharingDisabled() {
        return mIsSharingDisabled;
    }

    // Builder class
    public static class Builder {
        private @Nullable GroupToken mGroupToken;
        private @Nullable GURL mLearnAboutBlockedAccounts;
        private @Nullable GURL mActivityLogsUrl;
        private @Nullable ManageCallback mManageCallback;
        private @Nullable DataSharingUiConfig mCommonConfig;
        private boolean mIsSharingDisabled;

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
         * Sets the hyperlink for "learn about blocked accounts".
         *
         * @param learnAboutBlockedAccounts The hyperlink to learn about blocked accounts.
         */
        public Builder setLearnAboutBlockedAccounts(GURL learnAboutBlockedAccounts) {
            this.mLearnAboutBlockedAccounts = learnAboutBlockedAccounts;
            return this;
        }

        /**
         * Sets the hyperlink for viewing activity logs.
         *
         * @param activityLogsUrl The hyperlink for viewing activity logs.
         */
        public Builder setActivityLogsUrl(GURL activityLogsUrl) {
            this.mActivityLogsUrl = activityLogsUrl;
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

        /**
         * Sets whether sharing is disabled.
         *
         * @param isSharingDisabled True if sharing is disabled.
         */
        public Builder setIsSharingDisabled(boolean isSharingDisabled) {
            this.mIsSharingDisabled = isSharingDisabled;
            return this;
        }

        public DataSharingManageUiConfig build() {
            return new DataSharingManageUiConfig(this);
        }
    }
}
