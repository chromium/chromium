// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Datatype for describing initial window positioning requirements used in addNewContents calls. */
@NullMarked
public final class WindowFeatures {
    public final @Nullable Integer left;
    public final @Nullable Integer top;
    public final @Nullable Integer width;
    public final @Nullable Integer height;

    public WindowFeatures() {
        this(null, null, null, null);
    }

    public WindowFeatures(
            @Nullable Integer left,
            @Nullable Integer top,
            @Nullable Integer width,
            @Nullable Integer height) {
        this.left = left;
        this.top = top;
        this.width = width;
        this.height = height;
    }

    // More methods will be added in future CLs when needed by the Window Popup feature.
}
