// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.content.Context;

import org.chromium.url.GURL;

/** Config class for the Data Sharing UI. */
public class DataSharingUiConfig {

    // --- Form Factor Config ---
    private boolean mIsTablet;

    // --- Tab Group Details ---
    private String mTabGroupName;

    // --- Usage Config ---
    private Context mContext;
    private GURL mLearnMoreHyperLink;
    private DataSharingCallback mDataSharingCallback;

    /** Callback interface for common data sharing UI events. */
    public interface DataSharingCallback {
        default void onLearnMoreAboutSharedTabGroupsClicked(GURL url) {}
    }

    private DataSharingUiConfig(Builder builder) {
        this.mIsTablet = builder.mIsTablet;
        this.mContext = builder.mContext;
        this.mTabGroupName = builder.mTabGroupName;
        this.mLearnMoreHyperLink = builder.mLearnMoreHyperLink;
        this.mDataSharingCallback = builder.mDataSharingCallback;
    }

    public boolean getIsTablet() {
        return mIsTablet;
    }

    public Context getContext() {
        return mContext;
    }

    public String getTabGroupName() {
        return mTabGroupName;
    }

    public GURL getLearnMoreHyperLink() {
        return mLearnMoreHyperLink;
    }

    public DataSharingCallback getDataSharingCallback() {
        return mDataSharingCallback;
    }

    // Builder class
    public static class Builder {
        private boolean mIsTablet;
        private Context mContext;
        private String mTabGroupName;
        private GURL mLearnMoreHyperLink;
        private DataSharingCallback mDataSharingCallback;

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
