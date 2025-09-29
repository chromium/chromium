// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** A root view for web app header, it manages paddings and notifies about layout changes. */
@NullMarked
public class WebAppHeaderLayout extends FrameLayout implements View.OnLayoutChangeListener {

    private @Nullable Callback<Integer> mOnWidthChanged;
    private @Nullable Callback<Integer> mOnVisibilityChangedCallback;
    private final CutoutDrawable mBackgroundDrawable = new CutoutDrawable();

    public WebAppHeaderLayout(Context context) {
        super(context);
    }

    public WebAppHeaderLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        addOnLayoutChangeListener(this);
        setBackground(mBackgroundDrawable);
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (mOnWidthChanged == null) return;
        mOnWidthChanged.onResult(right - left);
    }

    public void setBackgroundDrawableColor(int color) {
        mBackgroundDrawable.setColor(color);
    }

    public void setBackgroundCutouts(List<Rect> cutouts) {
        mBackgroundDrawable.setCutouts(cutouts);
    }

    /**
     * Sets a callback that will be notified about width changes on the next layout pass.
     *
     * @param onWidthChanged a {@link Callback} that accepts new width.
     */
    public void setOnWidthChanged(@Nullable Callback<Integer> onWidthChanged) {
        mOnWidthChanged = onWidthChanged;
        if (mOnWidthChanged != null) {
            mOnWidthChanged.onResult(getWidth());
        }
    }

    /**
     * Callback that will be notified about visibility changes
     *
     * @param onVisibilityChangedCallback a {@link Callback} that accepts new visibility.
     */
    public void setOnVisibilityChangedCallback(
            @Nullable Callback<Integer> onVisibilityChangedCallback) {
        mOnVisibilityChangedCallback = onVisibilityChangedCallback;
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        // Make sure changedView == this, as any child view can trigger onvisibilityChanged.
        if (changedView == this && mOnVisibilityChangedCallback != null) {
            mOnVisibilityChangedCallback.onResult(visibility);
        }
    }

    /** Cleans up this view. */
    public void destroy() {
        removeOnLayoutChangeListener(this);
    }
}
