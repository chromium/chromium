// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.toolbar.ToolbarVariationUtils;

/** A location bar implementation specific for smaller/phone screens. */
@NullMarked
class LocationBarPhone extends LocationBarLayout {
    protected ImageButton mBackButton;
    protected @Nullable FrameLayout mOptionalButton;

    /** Constructor used to inflate from XML. */
    public LocationBarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);

        mBackButton = findViewById(R.id.omnibox_back_button);
        ViewStub optionalButtonStub = findViewById(R.id.optional_button_location_bar_stub);

        if (optionalButtonStub != null
                && ToolbarVariationUtils.isToolbarUiRefactorEnabled(context)) {
            mOptionalButton = (FrameLayout) optionalButtonStub.inflate();
            mOptionalButton.setVisibility(View.GONE);
            ConstraintLayout.LayoutParams params =
                    (ConstraintLayout.LayoutParams) mOptionalButton.getLayoutParams();
            params.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID;
            mOptionalButton.setLayoutParams(params);
        } else {
            mOptionalButton = null;
        }
    }

    @Override
    protected boolean isBackButtonVisible() {
        return mBackButton.getVisibility() == View.VISIBLE;
    }

    @Override
    /* package */ void setBackButtonVisibility(boolean shouldShow) {
        mBackButton.setVisibility(shouldShow ? VISIBLE : GONE);
        updateStartPadding();
    }

    @Override
    /* package */ void setBackButtonEnabled(boolean enabled) {
        mBackButton.setEnabled(enabled);
    }

    @Override
    /* package */ void setBackButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mBackButton, colorStateList);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onMeasure")) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    /**
     * Returns {@link MarginLayoutParams} of the LocationBar view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getLayoutParams()
     */
    public MarginLayoutParams getMarginLayoutParams() {
        return (MarginLayoutParams) getLayoutParams();
    }

    int getOffsetOfFirstVisibleFocusedView() {
        return 0;
    }
}
