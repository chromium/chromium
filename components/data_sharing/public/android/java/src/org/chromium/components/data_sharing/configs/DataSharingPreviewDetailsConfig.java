// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.data_sharing.configs;

import android.graphics.Bitmap;

import java.util.List;

/** Preview details images in join flow */
public final class DataSharingPreviewDetailsConfig {
    private final List<TabPreview> mTabPreviews;

    /** Preview entry for a tab in the details view */
    public static final class TabPreview {
        // The display URL for the tab, see TabPreview.displayUrl.
        public final String displayUrl;
        // Favicon for the tab, can be null.
        public final Bitmap favicon;

        public TabPreview(String displayUrl, Bitmap favicon) {
            this.displayUrl = displayUrl;
            this.favicon = favicon;
        }
    }

    private DataSharingPreviewDetailsConfig(List<TabPreview> tabPreviews) {
        this.mTabPreviews = tabPreviews;
    }

    public List<TabPreview> getTabPreviews() {
        return mTabPreviews;
    }

    // Builder class
    public static class Builder {
        private List<TabPreview> mTabPreviews;

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
