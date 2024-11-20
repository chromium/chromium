// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

/** Preview data for images in UI flows */
public final class DataSharingPreviewDataConfig {
    private final Bitmap mTabGroupPreviewImage;

    private DataSharingPreviewDataConfig(Bitmap tabGroupPreviewImage) {
        this.mTabGroupPreviewImage = tabGroupPreviewImage;
    }

    public Bitmap getTabGroupPreviewImage() {
        return mTabGroupPreviewImage;
    }

    // Builder class
    public static class Builder {
        private Bitmap mTabGroupPreviewImage;

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
