// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Custom view for the web content hairline container in Side UI. */
@NullMarked
@DoNotMock
/* package */ final class SideUiWebContentHairlineContainer extends FrameLayout {

    private ImageView mTopHairline;
    private ImageView mLeftHairline;
    private ImageView mRightHairline;

    private ImageView mTopLeftRoundedCorner;
    private ImageView mBottomLeftRoundedCorner;
    private ImageView mTopRightRoundedCorner;

    /** Constructor for inflating from XML. */
    public SideUiWebContentHairlineContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTopHairline = findViewById(R.id.top_web_content_hairline);
        mLeftHairline = findViewById(R.id.left_web_content_hairline);
        mRightHairline = findViewById(R.id.right_web_content_hairline);

        mTopLeftRoundedCorner = findViewById(R.id.top_left_web_content_rounded_corner);
        mBottomLeftRoundedCorner = findViewById(R.id.bottom_left_web_content_rounded_corner);
        mTopRightRoundedCorner = findViewById(R.id.top_right_web_content_rounded_corner);
    }

    /** Returns the top hairline ImageView. */
    /* package */ ImageView getTopHairline() {
        return mTopHairline;
    }

    /** Returns the left hairline ImageView. */
    /* package */ ImageView getLeftHairline() {
        return mLeftHairline;
    }

    /** Returns the right hairline ImageView. */
    /* package */ ImageView getRightHairline() {
        return mRightHairline;
    }

    /** Returns the top left rounded corner ImageView. */
    /* package */ ImageView getTopLeftRoundedCorner() {
        return mTopLeftRoundedCorner;
    }

    /** Returns the bottom left rounded corner ImageView. */
    /* package */ ImageView getBottomLeftRoundedCorner() {
        return mBottomLeftRoundedCorner;
    }

    /** Returns the top right rounded corner ImageView. */
    /* package */ ImageView getTopRightRoundedCorner() {
        return mTopRightRoundedCorner;
    }

    /**
     * Notifies this container of the Incognito state changing. Update the view's colors.
     *
     * @param isIncognito The new Incognito state.
     */
    /* package */ void setIncognitoState(boolean isIncognito) {
        Context context = getContext();

        @ColorInt
        int incognitoDividerColor =
                context.getColor(R.color.web_content_hairline_divider_color_incognito);
        ColorStateList tintList =
                isIncognito ? ColorStateList.valueOf(incognitoDividerColor) : null;
        mTopHairline.setImageTintList(tintList);
        mLeftHairline.setImageTintList(tintList);
        mRightHairline.setImageTintList(tintList);

        int topLeftResId =
                isIncognito
                        ? R.drawable.rounded_corner_left_incognito
                        : R.drawable.rounded_corner_left;
        int topRightResId =
                isIncognito
                        ? R.drawable.rounded_corner_right_incognito
                        : R.drawable.rounded_corner_right;
        mTopLeftRoundedCorner.setImageResource(topLeftResId);
        mTopRightRoundedCorner.setImageResource(topRightResId);

        // TODO(crbug.com/537032526): Update VT bottom-left corner for Incognito.
    }
}
