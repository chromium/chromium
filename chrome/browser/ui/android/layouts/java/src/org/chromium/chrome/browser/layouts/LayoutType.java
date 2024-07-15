// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The type info of the Layout. These types are bit flags, so they can be or-ed together to test for
 * multiple.
 */
@IntDef({
    LayoutType.NONE,
    LayoutType.BROWSING,
    LayoutType.TAB_SWITCHER,
    LayoutType.TOOLBAR_SWIPE,
    LayoutType.SIMPLE_ANIMATION,
    LayoutType.START_SURFACE
})
@Retention(RetentionPolicy.SOURCE)
public @interface LayoutType {
    int NONE = 0;
    int BROWSING = 1;
    int TAB_SWITCHER = 2;
    int TOOLBAR_SWIPE = 4;
    int SIMPLE_ANIMATION = 8;
    @Deprecated int START_SURFACE = 16;
    // Next layout type should be 32.
}
