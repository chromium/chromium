// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Encapsulates the state for a button in the Fusebox popup. */
@NullMarked
public class PopupButtonData {
    @IntDef({PopupButtonType.RECENT_TAB, PopupButtonType.TOOL, PopupButtonType.MODEL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PopupButtonType {
        int RECENT_TAB = 0;
        int TOOL = 1;
        int MODEL = 2;
    }

    public final Runnable onClicked;
    public final String text;
    // Either iconId (predefined vector drawable) or customIcon (bitmap favicon) is set.
    public final /*IconResourceIds*/ int iconId;
    public final @Nullable Bitmap customIcon;
    public final boolean enabled;
    public final boolean selected;
    public final @PopupButtonType int type;
    public final int protoId;
    public final boolean hasColor;
    public final String tooltip;

    private PopupButtonData(Builder builder) {
        this.onClicked = () -> builder.mOnClicked.onResult(this);
        this.text = builder.mText;
        this.iconId = builder.mIconId;
        this.customIcon = builder.mCustomIcon;
        this.enabled = builder.mEnabled;
        this.selected = builder.mSelected;
        this.type = builder.mType;
        this.protoId = builder.mProtoId;
        this.hasColor = builder.mHasColor;
        this.tooltip = builder.mTooltip;
    }

    /** Builder to construct {@link PopupButtonData} instances. */
    public static class Builder {
        private Callback<PopupButtonData> mOnClicked = (data) -> {};
        private String mText = "";
        private int mIconId;
        private @Nullable Bitmap mCustomIcon;
        private boolean mEnabled;
        private boolean mSelected;
        private @PopupButtonType int mType;
        private int mProtoId;
        private boolean mHasColor;
        private String mTooltip = "";

        public Builder setOnClicked(Callback<PopupButtonData> onClicked) {
            mOnClicked = onClicked;
            return this;
        }

        public Builder setText(String text) {
            mText = text;
            return this;
        }

        public Builder setIconId(int iconId) {
            assert mCustomIcon == null;
            mIconId = iconId;
            return this;
        }

        public Builder setCustomIcon(@Nullable Bitmap customIcon) {
            assert mIconId == 0;
            mCustomIcon = customIcon;
            return this;
        }

        public Builder setEnabled(boolean enabled) {
            mEnabled = enabled;
            return this;
        }

        public Builder setSelected(boolean selected) {
            mSelected = selected;
            return this;
        }

        public Builder setType(@PopupButtonType int type) {
            mType = type;
            return this;
        }

        public Builder setProtoId(int protoId) {
            mProtoId = protoId;
            return this;
        }

        public Builder setHasColor(boolean hasColor) {
            mHasColor = hasColor;
            return this;
        }

        public Builder setTooltip(String tooltip) {
            mTooltip = tooltip;
            return this;
        }

        public PopupButtonData build() {
            return new PopupButtonData(this);
        }
    }
}
