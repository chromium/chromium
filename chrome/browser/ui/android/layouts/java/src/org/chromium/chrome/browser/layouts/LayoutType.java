// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The type info of the Layout.
 */
@IntDef({LayoutType.BROWSING, LayoutType.TAB_SWITCHER, LayoutType.TOOLBAR_SWIPE,
        LayoutType.SIMPLE_ANIMATION})
@Retention(RetentionPolicy.SOURCE)
public @interface LayoutType {
    int BROWSING = 0;
    int TAB_SWITCHER = 1;
    int TOOLBAR_SWIPE = 2;
    int SIMPLE_ANIMATION = 3;
}
