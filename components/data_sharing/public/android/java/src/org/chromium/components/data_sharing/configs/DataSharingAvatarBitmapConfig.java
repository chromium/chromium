// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.data_sharing.GroupMember;

/** Config class for getting avatar as bitmap. */
@NullMarked
public final class DataSharingAvatarBitmapConfig {

    private final @Nullable Context mContext;
    private final @Nullable GroupMember mGroupMember;
    private final boolean mIsDarkMode;
    private final int mAvatarSizeInPixels;
    private final @ColorInt int mAvatarFallbackColor;
    private final @Nullable DataSharingAvatarCallback mDataSharingAvatarCallback;

    /** Interface used to pass the result of avatar loading. */
    @FunctionalInterface
    public interface DataSharingAvatarCallback {

        /**
         * Called when the avatar bitmap is ready.
         *
         * @param bitmap The loaded avatar bitmap. If might return null, if group member is invalid.
         */
        void onAvatarLoaded(Bitmap bitmap);
    }

    private DataSharingAvatarBitmapConfig(Builder builder) {
        this.mContext = builder.mContext;
        this.mGroupMember = builder.mGroupMember;
        this.mIsDarkMode = builder.mIsDarkMode;
        this.mAvatarSizeInPixels = builder.mAvatarSizeInPixels;
        this.mAvatarFallbackColor = builder.mAvatarFallbackColor;
        this.mDataSharingAvatarCallback = builder.mDataSharingAvatarCallback;
    }

    public @Nullable Context getContext() {
        return mContext;
    }

    public @Nullable GroupMember getGroupMember() {
        return mGroupMember;
    }

    public boolean isDarkMode() {
        return mIsDarkMode;
    }

    public int getAvatarSizeInPixels() {
        return mAvatarSizeInPixels;
    }

    public @ColorInt int getAvatarFallbackColor() {
        return mAvatarFallbackColor;
    }

    public @Nullable DataSharingAvatarCallback getDataSharingAvatarCallback() {
        return mDataSharingAvatarCallback;
    }

    /** Builder class for {@link DataSharingAvatarBitmapConfig}. */
    public static final class Builder {
        private @Nullable Context mContext;
        private @Nullable GroupMember mGroupMember;
        private boolean mIsDarkMode;
        private int mAvatarSizeInPixels;
        private @ColorInt int mAvatarFallbackColor;
        private @Nullable DataSharingAvatarCallback mDataSharingAvatarCallback;

        /**
         * Sets the application context.
         *
         * @param context The {@link Context}.
         */
        public Builder setContext(Context context) {
            this.mContext = context;
            return this;
        }

        /**
         * Sets the group member whose avatar should be fetched.
         *
         * @param groupMember The group member object. If null, returns a default fallback avatar.
         */
        public Builder setGroupMember(@Nullable GroupMember groupMember) {
            this.mGroupMember = groupMember;
            return this;
        }

        /**
         * Sets whether the avatar should be rendered in dark mode.
         *
         * @param isDarkMode {@code true} for dark mode, {@code false} otherwise.
         */
        public Builder setIsDarkMode(boolean isDarkMode) {
            this.mIsDarkMode = isDarkMode;
            return this;
        }

        /**
         * Sets the desired size of the avatar in pixels.
         *
         * @param avatarSizeInPixels The avatar size in pixels.
         */
        public Builder setAvatarSizeInPixels(int avatarSizeInPixels) {
            this.mAvatarSizeInPixels = avatarSizeInPixels;
            return this;
        }

        /**
         * Sets the fallback color for the avatar.This is used when monogram is shown.
         *
         * @param avatarFallbackColor The fallback color for avatar.
         */
        public Builder setAvatarFallbackColor(@ColorInt int avatarFallbackColor) {
            this.mAvatarFallbackColor = avatarFallbackColor;
            return this;
        }

        /**
         * Sets the {@link DataSharingAvatarCallback} to be notified when the avatar is loaded.
         *
         * @param dataSharingAvatarCallback The {@link DataSharingAvatarCallback}.
         * @return This builder.
         */
        public Builder setDataSharingAvatarCallback(
                DataSharingAvatarCallback dataSharingAvatarCallback) {
            this.mDataSharingAvatarCallback = dataSharingAvatarCallback;
            return this;
        }

        public DataSharingAvatarBitmapConfig build() {
            return new DataSharingAvatarBitmapConfig(this);
        }
    }
}
