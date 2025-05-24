// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Preview details images in join flow */
@NullMarked
public final class DataSharingPreviewDetailsConfig {
    private final @Nullable List<TabPreview> mTabPreviews;

    /** Preview entry for a tab in the details view */
    public static final class TabPreview {
        // The display URL for the tab, see TabPreview.displayUrl.
        public final String displayUrl;
        // Favicon for the tab, can be null.
        public final @Nullable Bitmap favicon;

        public TabPreview(String displayUrl, @Nullable Bitmap favicon) {
            this.displayUrl = displayUrl;
            this.favicon = favicon;
        }
    }

    private DataSharingPreviewDetailsConfig(@Nullable List<TabPreview> tabPreviews) {
        this.mTabPreviews = tabPreviews;
    }

    public @Nullable List<TabPreview> getTabPreviews() {
        return mTabPreviews;
    }

    // Builder class
    public static class Builder {
        private @Nullable List<TabPreview> mTabPreviews;

        /** The list of tab previews. */
        public Builder setTabPreviews(List<TabPreview> tabPreviews) {
            this.mTabPreviews = tabPreviews;
            return this;
        }

        public DataSharingPreviewDetailsConfig build() {
            return new DataSharingPreviewDetailsConfig(mTabPreviews);
        }
    }
}
