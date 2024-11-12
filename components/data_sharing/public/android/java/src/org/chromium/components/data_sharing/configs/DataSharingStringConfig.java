// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Config class for the Data Sharing UI Strings. */
public class DataSharingStringConfig {

    private Map<Integer, Integer> mResourceIds;

    public DataSharingStringConfig() {
        mResourceIds = new HashMap<>();
    }

    private DataSharingStringConfig(Builder builder) {
        this.mResourceIds = builder.mResourceIds;
    }

    public Integer getResourceId(@StringKey.Key int key) {
        return mResourceIds.get(key);
    }

    // Builder class
    public static class Builder {
        private Map<Integer, Integer> mResourceIds = new HashMap<>();

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
            MANAGE_DESCRIPTION,
            LET_ANYONE_JOIN_DESCRIPTION,
            REMOVE_MESSAGE,
            BLOCK_MESSAGE
        })
        public @interface Key {}

        public static final int CREATE_TITLE = 0;
        public static final int CREATE_DESCRIPTION = 1;
        public static final int JOIN_TITLE = 2;
        public static final int JOIN_DESCRIPTION = 3;
        public static final int JOIN_DETAILS_TITLE = 4;
        public static final int JOIN_DETAILS_HEADER = 5;
        public static final int MANAGE_DESCRIPTION = 6;
        public static final int LET_ANYONE_JOIN_DESCRIPTION = 7;
        public static final int REMOVE_MESSAGE = 8;
        public static final int BLOCK_MESSAGE = 9;
    }
}
