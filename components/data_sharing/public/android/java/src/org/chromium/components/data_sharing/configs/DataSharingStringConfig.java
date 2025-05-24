// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Config class for the Data Sharing UI Strings. */
@NullMarked
public class DataSharingStringConfig {

    private final Map<Integer, Integer> mResourceIds;

    public DataSharingStringConfig() {
        mResourceIds = new HashMap<>();
    }

    private DataSharingStringConfig(Builder builder) {
        this.mResourceIds = builder.mResourceIds;
    }

    public @Nullable Integer getResourceId(@StringKey.Key int key) {
        return mResourceIds.get(key);
    }

    public Map<Integer, Integer> getResourceIds() {
        return mResourceIds;
    }

    // Builder class
    public static class Builder {
        private final Map<Integer, Integer> mResourceIds = new HashMap<>();

        /**
         * Sets the resource ID for the given key.
         *
         * @param key The string key.
         * @param resourceId The resource ID to set.
         */
        public Builder setResourceId(@StringKey.Key int key, Integer resourceId) {
            this.mResourceIds.put(key, resourceId);
            return this;
        }

        public DataSharingStringConfig build() {
            return new DataSharingStringConfig(this);
        }
    }

    /** Keys that indicate which string is being requested. */
    public static class StringKey {

        @Retention(RetentionPolicy.SOURCE)
        @IntDef({
            CREATE_TITLE,
            CREATE_DESCRIPTION,
            JOIN_TITLE,
            JOIN_DESCRIPTION,
            JOIN_DETAILS_TITLE,
            JOIN_DETAILS_HEADER,
            MANAGE_TITLE,
            MANAGE_HEADER,
            MANAGE_DESCRIPTION,
            LET_ANYONE_JOIN_DESCRIPTION,
            BLOCK_MESSAGE,
            BLOCK_AND_LEAVE_GROUP_MESSAGE,
            REMOVE_MESSAGE,
            DELETE_GROUP_MESSAGE,
            LEAVE_GROUP_MESSAGE,
            STOP_SHARING_MESSAGE,
            JOIN_TITLE_SINGLE,
            TABS_COUNT_TITLE,
            LEARN_ABOUT_SHARED_TAB_GROUPS,
            LEARN_ABOUT_BLOCKED_ACCOUNTS,
            JOIN_GROUP_IS_FULL_ERROR_TITLE,
            JOIN_GROUP_IS_FULL_ERROR_BODY,
            ACTIVITY_LOGS_TITLE,
            LET_ANYONE_JOIN_GROUP_WHEN_FULL_DESCRIPTION,
        })
        public @interface Key {}

        public static final int CREATE_TITLE = 0;
        public static final int CREATE_DESCRIPTION = 1;
        public static final int JOIN_TITLE = 2;
        public static final int JOIN_DESCRIPTION = 3;
        public static final int JOIN_DETAILS_TITLE = 4;
        public static final int JOIN_DETAILS_HEADER = 5;
        public static final int MANAGE_TITLE = 6;
        public static final int MANAGE_HEADER = 7;
        public static final int MANAGE_DESCRIPTION = 8;
        public static final int LET_ANYONE_JOIN_DESCRIPTION = 9;
        public static final int BLOCK_MESSAGE = 10;
        public static final int BLOCK_AND_LEAVE_GROUP_MESSAGE = 11;
        public static final int REMOVE_MESSAGE = 12;
        public static final int DELETE_GROUP_MESSAGE = 13;
        public static final int LEAVE_GROUP_MESSAGE = 14;
        public static final int STOP_SHARING_MESSAGE = 15;
        public static final int JOIN_TITLE_SINGLE = 16;
        public static final int TABS_COUNT_TITLE = 17;
        public static final int LEARN_ABOUT_SHARED_TAB_GROUPS = 18;
        public static final int LEARN_ABOUT_BLOCKED_ACCOUNTS = 19;
        public static final int JOIN_GROUP_IS_FULL_ERROR_TITLE = 20;
        public static final int JOIN_GROUP_IS_FULL_ERROR_BODY = 21;
        public static final int ACTIVITY_LOGS_TITLE = 22;
        public static final int LET_ANYONE_JOIN_GROUP_WHEN_FULL_DESCRIPTION = 23;
    }
}
