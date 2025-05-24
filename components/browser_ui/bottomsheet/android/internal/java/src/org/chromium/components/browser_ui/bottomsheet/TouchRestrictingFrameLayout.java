// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A specialized FrameLayout that is capable of ignoring all user input based on the state of
 * the bottom sheet.
 */
@NullMarked
class TouchRestrictingFrameLayout extends FrameLayout {
    /** A handle to the bottom sheet. */
    private @Nullable BottomSheet mBottomSheet;

    public TouchRestrictingFrameLayout(Context context, AttributeSet atts) {
        super(context, atts);
    }

    /** @param sheet The bottom sheet. */
    public void setBottomSheet(BottomSheet sheet) {
        mBottomSheet = sheet;
    }

    /** @return Whether touch is enabled. */
    private boolean isTouchDisabled() {
        return mBottomSheet == null
                || mBottomSheet.getSheetState() == BottomSheetController.SheetState.SCROLLING;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (isTouchDisabled()) return true;
        return super.onInterceptTouchEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (isTouchDisabled()) return true;
        return super.onTouchEvent(event);
    }
}
