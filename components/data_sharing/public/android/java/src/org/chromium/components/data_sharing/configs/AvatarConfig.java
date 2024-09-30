// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.GroupMember;

import java.util.List;

/** Config class for Avatars UI. */
public class AvatarConfig {

    // Properties for avatar customization.
    // The size of the avatar icon in pixels.
    private int mAvatarSizeInPixels;
    // The background color of the avatar icon.
    private @ColorInt int mAvatarBackgroundColor;
    // The color of the border around the avatar icon.
    private @ColorInt int mBorderColor;
    // The width of the border around the avatar icon in pixels.
    private int mBorderWidthInPixels;

    // Properties for showing avatars.
    private Context mContext;
    private List<GroupMemberAvatarView> mGroupMemberAvatarViews;

    public static class GroupMemberAvatarView {

        private ViewGroup mView;
        private GroupMember mGroupMember;

        public GroupMemberAvatarView(ViewGroup view, GroupMember member) {
            this.mView = view;
            this.mGroupMember = member;
        }

        public ViewGroup getView() {
            return mView;
        }

        public GroupMember getGroupMember() {
            return mGroupMember;
        }
    }

    // TODO (ritikagup) : If this is not called, remove it.
    private Callback<Boolean> mSuccessCallback;

    private AvatarConfig(Builder builder) {
        this.mAvatarSizeInPixels = builder.mAvatarSizeInPixels;
        this.mAvatarBackgroundColor = builder.mAvatarBackgroundColor;
        this.mBorderColor = builder.mBorderColor;
        this.mBorderWidthInPixels = builder.mBorderWidthInPixels;
        this.mContext = builder.mContext;
        this.mGroupMemberAvatarViews = builder.mGroupMemberAvatarViews;
        this.mSuccessCallback = builder.mSuccessCallback;
    }

    public int getAvatarSizeInPixels() {
        return mAvatarSizeInPixels;
    }

    public @ColorInt int getAvatarBackgroundColor() {
        return mAvatarBackgroundColor;
    }

    public @ColorInt int getBorderColor() {
        return mBorderColor;
    }

    public int getBorderWidthInPixels() {
        return mBorderWidthInPixels;
    }

    public Context getContext() {
        return mContext;
    }

    public List<GroupMemberAvatarView> getGroupMemberAvatarViews() {
        return mGroupMemberAvatarViews;
    }

    public Callback<Boolean> getSuccessCallback() {
        return mSuccessCallback;
    }

    /** Builder for {@link AvatarConfig}. */
    public static class Builder {
        private int mAvatarSizeInPixels;
        private @ColorInt int mAvatarBackgroundColor;
        private @ColorInt int mBorderColor;
        private int mBorderWidthInPixels;
        private Context mContext;
        private List<GroupMemberAvatarView> mGroupMemberAvatarViews;
        private Callback<Boolean> mSuccessCallback;

        /** Sets avatar icon size in pixels. */
        public Builder setAvatarSizeInPixels(int avatarSizeInPixels) {
            this.mAvatarSizeInPixels = avatarSizeInPixels;
            return this;
        }

        /** Sets background color for the avatar icon. */
        public Builder setAvatarBackgroundColor(@ColorInt int avatarBackgroundColor) {
            this.mAvatarBackgroundColor = avatarBackgroundColor;
            return this;
        }

        /** Sets border colour value for the avatar icon. */
        public Builder setBorderColor(@ColorInt int borderColor) {
            this.mBorderColor = borderColor;
            return this;
        }

        /** Sets border width in pixels for avatar icon. */
        public Builder setBorderWidthInPixels(int borderWidthInPixels) {
            this.mBorderWidthInPixels = borderWidthInPixels;
            return this;
        }

        /** Sets current android context. */
        public Builder setContext(Context context) {
            this.mContext = context;
            return this;
        }

        /** Sets group members and avatar views for them. */
        public Builder setGroupMemberAvatarViews(
                List<GroupMemberAvatarView> groupMemberAvatarViews) {
            this.mGroupMemberAvatarViews = groupMemberAvatarViews;
            return this;
        }

        public Builder setSuccessCallback(Callback<Boolean> successCallback) {
            this.mSuccessCallback = successCallback;
            return this;
        }

        public AvatarConfig build() {
            return new AvatarConfig(this);
        }
    }
}