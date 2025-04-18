// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.web_app_header;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public class WebAppHeaderLayout extends FrameLayout implements View.OnLayoutChangeListener {

    private @Nullable Callback<Integer> mOnWidthChanged;

    public WebAppHeaderLayout(@NonNull Context context) {
        super(context);
    }

    public WebAppHeaderLayout(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
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

    public void setOnWidthChanged(@Nullable Callback<Integer> onWidthChanged) {
        mOnWidthChanged = onWidthChanged;
        if (mOnWidthChanged != null) {
            mOnWidthChanged.onResult(getWidth());
        }
    }
}
