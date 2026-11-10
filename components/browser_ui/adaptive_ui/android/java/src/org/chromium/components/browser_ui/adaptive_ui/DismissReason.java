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
})
@Retention(RetentionPolicy.SOURCE)
public @interface DismissReason {
    /** The user pressed the system back button or gesture. */
    int BACK_PRESS = 0;

    /** The user tapped the scrim outside the sheet or dialog. */
    int TOUCH_OUTSIDE = 1;

    /** The user swiped the bottom sheet down to close it. Only reported by bottom sheets. */
    int SWIPE_DOWN = 2;

    /** The user tapped the explicit close ('X') button in the header. */
    int CLOSE_BUTTON = 3;

    /** The user activated the primary action button (or completed the primary content action). */
    int PRIMARY_ACTION = 4;

    /** The user activated the secondary (negative/cancel) action button. */
    int SECONDARY_ACTION = 5;

    /** The page navigated, or the container closed the entry for a reason it did not report. */
    int NAVIGATION = 6;

    /** The user switched away from the tab that owned the entry. Only reported by dialogs. */
    int TAB_SWITCHED = 7;

    /**
     * The tab, activity, or window hosting the entry was closed or destroyed. Only reported by
     * dialogs.
     */
    int TAB_CLOSED = 8;

    int COUNT = 9;
}
// LINT.ThenChange(//components/browser_ui/adaptive_ui/android/java/src/org/chromium/components/browser_ui/adaptive_ui/AdaptiveUiDialogCoordinator.java:DialogDismissReasonMapping,
// //components/browser_ui/adaptive_ui/android/java/src/org/chromium/components/browser_ui/adaptive_ui/AdaptiveUiSheetCoordinator.java:SheetDismissReasonMapping)
