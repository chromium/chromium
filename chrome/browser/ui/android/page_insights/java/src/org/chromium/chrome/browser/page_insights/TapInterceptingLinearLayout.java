// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.LinearLayout;

/** A {@link LinearLayout} that can intercept taps and prevent them being handled by child views. */
// TODO(b/306377148): Remove this once a solution is built into bottom sheet infra.
class TapInterceptingLinearLayout extends LinearLayout {

    private PageInsightsSheetContent.OnBottomSheetTapHandler mOnTapHandler;

    public TapInterceptingLinearLayout(Context context, AttributeSet atts) {
        super(context, atts);
    }

    void setOnTapHandler(PageInsightsSheetContent.OnBottomSheetTapHandler onTapHandler) {
        mOnTapHandler = onTapHandler;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP
                && mOnTapHandler != null
                && mOnTapHandler.handle()) {
            return true;
        }
        return super.onInterceptTouchEvent(event);
    }
}
