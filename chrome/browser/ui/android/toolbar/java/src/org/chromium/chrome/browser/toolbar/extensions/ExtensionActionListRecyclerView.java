// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View.MeasureSpec;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.extensions.R;

/**
 * A {@link RecyclerView} for extension action buttons. This container will automatically hide any
 * buttons that overflow the available width.
 */
@NullMarked
public class ExtensionActionListRecyclerView extends RecyclerView {

    public ExtensionActionListRecyclerView(Context context) {
        super(context);
    }

    public ExtensionActionListRecyclerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public ExtensionActionListRecyclerView(
            Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // The parent passes the window width as the maximum width.
        assert MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.AT_MOST;

        int windowWidth = MeasureSpec.getSize(widthMeasureSpec);
        int reservedWidth =
                getResources().getDimensionPixelSize(R.dimen.extension_toolbar_baseline_width);
        int effectiveWidth = windowWidth - reservedWidth - getPaddingLeft() - getPaddingRight();

        int itemWidth =
                getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.toolbar.R.dimen.toolbar_button_width);
        assert itemWidth > 0;

        int numItems = 0;
        if (effectiveWidth > 0) {
            numItems = effectiveWidth / itemWidth;
        }

        if (numItems > 0) {
            int newWidth = (numItems * itemWidth) + getPaddingLeft() + getPaddingRight();
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(newWidth, MeasureSpec.AT_MOST), heightMeasureSpec);
        } else {
            super.onMeasure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.EXACTLY), heightMeasureSpec);
        }
    }
}
