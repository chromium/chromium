// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.content.Context;
import android.graphics.Bitmap;

/** Config class for getting avatar as bitmap. */
public final class DataSharingAvatarBitmapConfig {

    private final Context mContext;
    private final String mPhotoUrl;
    private final String mDisplayName;
    private final boolean mIsDarkMode;
    private final int mAvatarSizeInPixels;
    private final DataSharingAvatarCallback mDataSharingAvatarCallback;

    /** Interface used to pass the result of avatar loading. */
    public interface DataSharingAvatarCallback {

        /**
         * Called when the avatar bitmap is ready.
         *
         * @param bitmap The loaded avatar bitmap.
         */
        default void onAvatarLoaded(Bitmap bitmap) {}
    }

    private DataSharingAvatarBitmapConfig(Builder builder) {
        this.mContext = builder.mContext;
        this.mPhotoUrl = builder.mPhotoUrl;
        this.mDisplayName = builder.mDisplayName;
        this.mIsDarkMode = builder.mIsDarkMode;
        this.mAvatarSizeInPixels = builder.mAvatarSizeInPixels;
        this.mDataSharingAvatarCallback = builder.mDataSharingAvatarCallback;
    }

    public Context getContext() {
        return mContext;
    }

    public String getPhotoUrl() {
        return mPhotoUrl;
    }

    public String getDisplayName() {
        return mDisplayName;
    }

    public boolean isDarkMode() {
        return mIsDarkMode;
    }

    public int getAvatarSizeInPixels() {
        return mAvatarSizeInPixels;
    }

    public DataSharingAvatarCallback getDataSharingAvatarCallback() {
        return mDataSharingAvatarCallback;
    }

    /** Builder class for {@link DataSharingAvatarBitmapConfig}. */
    public static final class Builder {
        private Context mContext;
        private String mPhotoUrl;
        private String mDisplayName;
        private boolean mIsDarkMode;
        private int mAvatarSizeInPixels;
        private DataSharingAvatarCallback mDataSharingAvatarCallback;

        /**
         * Sets the {@link Context} to use for loading resources.
         *
         * @param context The {@link Context}.
         */
        public Builder setContext(Context context) {
            this.mContext = context;
            return this;
        }

        /**
         * Sets the URL of the photo to load.
         *
         * @param photoUrl The URL of the photo.
         */
        public Builder setPhotoUrl(String photoUrl) {
            this.mPhotoUrl = photoUrl;
            return this;
        }

        /**
         * Sets the display name associated with the avatar.
         *
         * @param displayName The display name.
         */
        public Builder setDisplayName(String displayName) {
            this.mDisplayName = displayName;
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
