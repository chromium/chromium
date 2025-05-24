// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Preview data for images in UI flows */
@NullMarked
public final class DataSharingPreviewDataConfig {
    private final @Nullable Bitmap mTabGroupPreviewImage;

    private DataSharingPreviewDataConfig(@Nullable Bitmap tabGroupPreviewImage) {
        this.mTabGroupPreviewImage = tabGroupPreviewImage;
    }

    public @Nullable Bitmap getTabGroupPreviewImage() {
        return mTabGroupPreviewImage;
    }

    // Builder class
    public static class Builder {
        private @Nullable Bitmap mTabGroupPreviewImage;

        /** The preview image bitmap */
        public Builder setTabGroupPreviewImage(Bitmap tabGroupPreviewImage) {
            this.mTabGroupPreviewImage = tabGroupPreviewImage;
            return this;
        }

        public DataSharingPreviewDataConfig build() {
            return new DataSharingPreviewDataConfig(mTabGroupPreviewImage);
        }
    }
}
