// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

/** Provides the configuration params required by the tiles UI. */
public class TileConfig {
    public final String umaPrefix;

    /** Constructor. */
    private TileConfig(Builder builder) {
        umaPrefix = builder.mUmaPrefix;
    }

    /** Helper class for building a {@link TileConfig}. */
    public static class Builder {
        private String mUmaPrefix;

        /**
         * Sets the histogram prefix to be used while collecting metrics.
         * @param umaPrefix The prefix to be used for histograms.
         * @return A {@link Builder} instance.
         */
        public Builder setUmaPrefix(String umaPrefix) {
            mUmaPrefix = umaPrefix;
            return this;
        }

        public TileConfig build() {
            return new TileConfig(this);
        }
    }
}