// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.widget.LinearLayout;

/**
 * A {@link LinearLayout} that can intercept touches and prevent them being handled by child views.
 */
// TODO(b/306377148): Remove this once a solution is built into bottom sheet infra.
class TouchInterceptingLinearLayout extends LinearLayout {

    private PageInsightsSheetContent.OnBottomSheetTouchHandler mOnTouchHandler;

    public TouchInterceptingLinearLayout(Context context, AttributeSet atts) {
        super(context, atts);
    }

    void setOnTouchHandler(PageInsightsSheetContent.OnBottomSheetTouchHandler onTouchHandler) {
        mOnTouchHandler = onTouchHandler;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        if (mOnTouchHandler != null && mOnTouchHandler.shouldInterceptTouchEvents()) {
            if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                mOnTouchHandler.handleTap();
            }
            return true;
        }
        return super.onInterceptTouchEvent(event);
    }
}
