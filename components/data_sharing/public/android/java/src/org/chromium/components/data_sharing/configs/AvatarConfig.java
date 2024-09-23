// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;
import androidx.annotation.ColorInt;

/** Config class for Avatars UI. */
public class AvatarConfig {

    // Properties for avatar customization
    private int mAvatarSizeInPixels;
    private @ColorInt int mAvatarBackgroundColor;
    private @ColorInt int mBorderColor;
    private int mBorderWidthInPixels;

    private AvatarConfig(Builder builder) {
        this.mAvatarSizeInPixels = builder.mAvatarSizeInPixels;
        this.mAvatarBackgroundColor = builder.mAvatarBackgroundColor;
        this.mBorderColor = builder.mBorderColor;
        this.mBorderWidthInPixels = builder.mBorderWidthInPixels;
    }

    /** Returns avatar icon size in pixels. */
    public int getAvatarSizeInPixels() {
        return mAvatarSizeInPixels;
    }

    /** Returns background color for the avatar icon. */
    public @ColorInt int getAvatarBackgroundColor() {
        return mAvatarBackgroundColor;
    }

    /** Returns border colour value for the avatar icon. */
    public @ColorInt int getBorderColor() {
        return mBorderColor;
    }

    /** Returns border width in pixels for avatar icon. */
    public int getBorderWidthInPixels() {
        return mBorderWidthInPixels;
    }

    // Setters for the properties (since they are non-final)
    public void setAvatarSizeInPixels(int avatarSizeInPixels) {
        this.mAvatarSizeInPixels = avatarSizeInPixels;
    }

    public void setAvatarBackgroundColor(@ColorInt int avatarBackgroundColor) {
        this.mAvatarBackgroundColor = avatarBackgroundColor;
    }

    public void setBorderColor(@ColorInt int borderColor) {
        this.mBorderColor = borderColor;
    }

    public void setBorderWidthInPixels(int borderWidthInPixels) {
        this.mBorderWidthInPixels = borderWidthInPixels;
    }

    /** Builder for {@link AvatarConfig}. */
    public static class Builder {
        private int mAvatarSizeInPixels;
        private @ColorInt int mAvatarBackgroundColor;
        private @ColorInt int mBorderColor;
        private int mBorderWidthInPixels;

        public Builder setAvatarSizeInPixels(int avatarSizeInPixels) {
            this.mAvatarSizeInPixels = avatarSizeInPixels;
            return this;
        }

        public Builder setAvatarBackgroundColor(@ColorInt int avatarBackgroundColor) {
            this.mAvatarBackgroundColor = avatarBackgroundColor;
            return this;
        }

        public Builder setBorderColor(@ColorInt int borderColor) {
            this.mBorderColor = borderColor;
            return this;
        }

        public Builder setBorderWidthInPixels(int borderWidthInPixels) {
            this.mBorderWidthInPixels = borderWidthInPixels;
            return this;
        }

        public AvatarConfig build() {
            return new AvatarConfig(this);
        }
    }
}