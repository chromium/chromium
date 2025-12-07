// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;

/**
 * Data model for a selectable item to be displayed by a {@link RichRadioButton} component,
 * featuring an optional icon, a required title, and an optional description.
 */
@NullMarked
public class RichRadioButtonData {
    public final @NonNull String id;
    public final @DrawableRes int iconResId;
    public final @NonNull String title;
    public final @Nullable String description;

    private RichRadioButtonData(Builder builder) {
        this.id = builder.mId;
        this.iconResId = builder.mIconResId;
        this.title = builder.mTitle;
        this.description = builder.mDescription;
    }

    public static class Builder {
        private final @NonNull String mId;
        private final @NonNull String mTitle;

        private @DrawableRes int mIconResId;
        private @Nullable String mDescription;

        public Builder(@NonNull String id, @NonNull String title) {
            this.mId = id;
            this.mTitle = title;
        }

        public Builder setIconResId(@DrawableRes int iconResId) {
            this.mIconResId = iconResId;
            return this;
        }

        public Builder setDescription(@Nullable String description) {
            this.mDescription = description;
            return this;
        }

        public @NonNull RichRadioButtonData build() {
            return new RichRadioButtonData(this);
        }
    }
}
