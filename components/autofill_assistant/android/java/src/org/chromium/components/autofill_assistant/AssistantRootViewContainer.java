// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * A special linear layout that limits its maximum size to always stay below the Chrome navigation
 * bar.
 */
public class AssistantRootViewContainer
        extends LinearLayout implements AssistantBrowserControls.Observer {
    private final Activity mActivity;
    private AssistantBrowserControls mBrowserControls;
    private AccessibilityUtil mAccessibilityUtil;
    private Rect mVisibleViewportRect = new Rect();
    private float mTalkbackSheetSizeFraction;
    private boolean mTalkbackResizingDisabled;

    public AssistantRootViewContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mActivity = ContextUtils.activityFromContext(context);
    }

    /** Initializes the object with the given {@link AssistantBrowserControls}. */
    public void initialize(@NonNull AssistantBrowserControlsFactory browserControlsFactory,
            AccessibilityUtil accessibilityUtil) {
        mBrowserControls = browserControlsFactory.createBrowserControls();
        mBrowserControls.setObserver(this);
        mAccessibilityUtil = accessibilityUtil;
    }

    public void setAccessibilityUtil(AccessibilityUtil accessibilityUtil) {
        mAccessibilityUtil = accessibilityUtil;
    }

    public void setTalkbackViewSizeFraction(float fraction) {
        mTalkbackSheetSizeFraction = fraction;
    }

    @Override
    public void onControlsOffsetChanged() {
        invalidate();
    }

    @Override
    public void onBottomControlsHeightChanged() {
        invalidate();
    }

    public void disableTalkbackViewResizing() {
        mTalkbackResizingDisabled = true;
    }

    void destroy() {
        if (mBrowserControls != null) {
            mBrowserControls.destroy();
        }
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
        int browserControlsOffset = mBrowserControls == null
                ? 0
                : -mBrowserControls.getContentOffset() - mBrowserControls.getBottomControlsHeight()
                        - mBrowserControls.getBottomControlOffset();
        int availableHeight = mVisibleViewportRect.height() - browserControlsOffset;

        int targetHeight;
        int mode;
        if (mAccessibilityUtil.isAccessibilityEnabled() && !mTalkbackResizingDisabled) {
            // TODO(b/143944870): Make this more stable with landscape mode.
            targetHeight = (int) (availableHeight * mTalkbackSheetSizeFraction);
            mode = MeasureSpec.EXACTLY;
        } else {
            targetHeight = Math.min(MeasureSpec.getSize(heightMeasureSpec), availableHeight);
            mode = MeasureSpec.AT_MOST;
        }
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(targetHeight, mode));
    }
}
