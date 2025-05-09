// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Config class for the Data Sharing UI. */
@NullMarked
public class DataSharingUiConfig {

    /** Enum for user actions. */
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(DataSharingUserAction)
    @IntDef({
        DataSharingUserAction.SHARE_FLOW_SHARE_LINK,
        DataSharingUserAction.SHARE_FLOW_OPEN_LEARN_MORE,
        DataSharingUserAction.JOIN_FLOW_JOIN_AND_OPEN,
        DataSharingUserAction.JOIN_FLOW_OPEN_LEARN_MORE,
        DataSharingUserAction.MANAGE_FLOW_SHARE_LINK,
        DataSharingUserAction.MANAGE_FLOW_LEAVE_GROUP,
        DataSharingUserAction.MANAGE_FLOW_BLOCK_PERSON,
        DataSharingUserAction.MANAGE_FLOW_BLOCK_AND_LEAVE_GROUP,
        DataSharingUserAction.MANAGE_FLOW_REMOVE_PERSON,
        DataSharingUserAction.MANAGE_FLOW_STOP_SHARING,
        DataSharingUserAction.MANAGE_FLOW_OPEN_LEARN_MORE,
        DataSharingUserAction.MANAGE_FLOW_SHOW_ACTIVITY_LOGS,
        DataSharingUserAction.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DataSharingUserAction {
        int SHARE_FLOW_SHARE_LINK = 0;
        int SHARE_FLOW_OPEN_LEARN_MORE = 1;
        int JOIN_FLOW_JOIN_AND_OPEN = 2;
        int JOIN_FLOW_OPEN_LEARN_MORE = 3;
        int MANAGE_FLOW_SHARE_LINK = 4;
        int MANAGE_FLOW_LEAVE_GROUP = 5;
        int MANAGE_FLOW_BLOCK_PERSON = 6;
        int MANAGE_FLOW_BLOCK_AND_LEAVE_GROUP = 7;
        int MANAGE_FLOW_REMOVE_PERSON = 8;
        int MANAGE_FLOW_STOP_SHARING = 9;
        int MANAGE_FLOW_OPEN_LEARN_MORE = 10;
        int MANAGE_FLOW_SHOW_ACTIVITY_LOGS = 11;
        int COUNT = 12;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/data_sharing/enums.xml:DataSharingUserAction)

    // --- Form Factor Config ---
    private final boolean mIsTablet;

    // --- Tab Group Details ---
    private final @Nullable String mTabGroupName;

    // --- Usage Config ---
    private final @Nullable Context mContext;
    private final @Nullable Activity mActivity;
    private final @Nullable GURL mLearnMoreHyperLink;
    private final @Nullable DataSharingStringConfig mDataSharingStringConfig;
    private final @Nullable DataSharingCallback mDataSharingCallback;

    /** Callback interface for common data sharing UI events. */
    public interface DataSharingCallback {
        default void onClickOpenChromeCustomTab(Context context, GURL url) {}

        default void recordUserActionClicks(@DataSharingUserAction int dataSharingUserAction) {}
    }

    private DataSharingUiConfig(Builder builder) {
        this.mIsTablet = builder.mIsTablet;
        this.mContext = builder.mContext;
        this.mActivity = builder.mActivity;
        this.mTabGroupName = builder.mTabGroupName;
        this.mLearnMoreHyperLink = builder.mLearnMoreHyperLink;
        this.mDataSharingStringConfig = builder.mDataSharingStringConfig;
        this.mDataSharingCallback = builder.mDataSharingCallback;
    }

    public boolean getIsTablet() {
        return mIsTablet;
    }

    public @Nullable Context getContext() {
        return mContext;
    }

    public @Nullable Activity getActivity() {
        return mActivity;
    }

    public @Nullable String getTabGroupName() {
        return mTabGroupName;
    }

    public @Nullable GURL getLearnMoreHyperLink() {
        return mLearnMoreHyperLink;
    }

    public @Nullable DataSharingStringConfig getDataSharingStringConfig() {
        return mDataSharingStringConfig;
    }

    public @Nullable DataSharingCallback getDataSharingCallback() {
        return mDataSharingCallback;
    }

    // Builder class
    public static class Builder {
        private boolean mIsTablet;
        private @Nullable Context mContext;
        private @Nullable Activity mActivity;
        private @Nullable String mTabGroupName;
        private @Nullable GURL mLearnMoreHyperLink;
        private @Nullable DataSharingStringConfig mDataSharingStringConfig;
        private @Nullable DataSharingCallback mDataSharingCallback;

        /**
         * Sets whether the device is a tablet.
         *
         * @param isTablet True if the device is a tablet.
         */
        public Builder setIsTablet(boolean isTablet) {
            this.mIsTablet = isTablet;
            return this;
        }

        /**
         * Sets the current context.
         *
         * @param context The current android context.
         */
        public Builder setContext(Context context) {
            this.mContext = context;
            return this;
        }

        /**
         * Sets the current android activity.
         *
         * @param activity The current android activity.
         */
        public Builder setActivity(Activity activity) {
            this.mActivity = activity;
            return this;
        }

        /**
         * Sets the name of the tab group.
         *
         * @param tabGroupName The name of the tab group.
         */
        public Builder setTabGroupName(String tabGroupName) {
            this.mTabGroupName = tabGroupName;
            return this;
        }

        /**
         * Sets the hyperlink for "learn more about shared tab groups".
         *
         * @param learnMoreHyperLink The hyperlink to learn more section.
         */
        public Builder setLearnMoreHyperLink(GURL learnMoreHyperLink) {
            this.mLearnMoreHyperLink = learnMoreHyperLink;
            return this;
        }

        /**
         * Sets the data sharing string config.
         *
         * @param dataSharingStringConfig The data sharing string configuration.
         */
        public Builder setDataSharingStringConfig(DataSharingStringConfig dataSharingStringConfig) {
            this.mDataSharingStringConfig = dataSharingStringConfig;
            return this;
        }

        /**
         * Sets the callback for data sharing UI events.
         *
         * @param dataSharingCallback The callback for data sharing UI events.
         */
        public Builder setDataSharingCallback(DataSharingCallback dataSharingCallback) {
            this.mDataSharingCallback = dataSharingCallback;
            return this;
        }

        public DataSharingUiConfig build() {
            return new DataSharingUiConfig(this);
        }
    }
}
