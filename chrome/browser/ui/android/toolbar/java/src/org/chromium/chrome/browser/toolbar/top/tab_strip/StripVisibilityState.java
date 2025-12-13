// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_strip;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * A set of states that represent the visibility of the tab strip. These types are bit flags, so
 * they can be or-ed together to test for multiple. Please use bitwise operations to check for a
 * specific visibility state other than VISIBLE.
 */
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    StripVisibilityState.VISIBLE,
    StripVisibilityState.OBSCURED,
    StripVisibilityState.HIDDEN_BY_SCROLL,
    StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION,
    StripVisibilityState.HIDDEN_BY_FADE,
})
@NullMarked
@Target({ElementType.TYPE_USE})
public @interface StripVisibilityState {
    /** Strip is visible. */
    int VISIBLE = 0;

    /** Strip is obscured by tab switcher. */
    int OBSCURED = 1;

    /** Strip is hidden by scroll. */
    int HIDDEN_BY_SCROLL = 2;

    /** Strip is hidden by a height transition. */
    int HIDDEN_BY_HEIGHT_TRANSITION = 4;

    /** Strip is hidden by an in-place fade transition. */
    int HIDDEN_BY_FADE = 8;
}
