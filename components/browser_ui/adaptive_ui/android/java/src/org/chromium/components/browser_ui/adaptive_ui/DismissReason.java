// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// One-way: DialogDismissalCause and StateChangeReason changes require updating this IntDef.
// LINT.IfChange(DismissReason)
/** Reasons for dismissing an adaptive UI entry. */
@NullMarked
@IntDef({
    DismissReason.BACK_PRESS,
    DismissReason.TOUCH_OUTSIDE,
    DismissReason.SWIPE_DOWN,
    DismissReason.CLOSE_BUTTON,
    DismissReason.PRIMARY_ACTION,
    DismissReason.SECONDARY_ACTION,
    DismissReason.NAVIGATION,
    DismissReason.TAB_SWITCHED,
    DismissReason.TAB_CLOSED,
    DismissReason.REPLACED,
})
@Retention(RetentionPolicy.SOURCE)
public @interface DismissReason {
    int BACK_PRESS = 0;
    int TOUCH_OUTSIDE = 1;
    int SWIPE_DOWN = 2;
    int CLOSE_BUTTON = 3;
    int PRIMARY_ACTION = 4;
    int SECONDARY_ACTION = 5;
    int NAVIGATION = 6;
    int TAB_SWITCHED = 7;
    int TAB_CLOSED = 8;
    int REPLACED = 9;
    int COUNT = 10;
}
// LINT.ThenChange()
