// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A set of states that represent the visibility of the tab strip. */
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    StripVisibilityState.UNKNOWN,
    StripVisibilityState.VISIBLE,
    StripVisibilityState.GONE,
    StripVisibilityState.INVISIBLE,
})
public @interface StripVisibilityState {
    /** Strip visibility is unknown. */
    int UNKNOWN = 0;

    /** Strip is visible. */
    int VISIBLE = 1;

    /** Strip is hidden by a height transition. */
    int GONE = 2;

    /** Strip is hidden by an in-place fade transition. */
    int INVISIBLE = 3;
}
