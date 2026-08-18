// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;

/** Presentation view layer for the BottomSheet component. */
@NullMarked
public class BottomSheetView extends FrameLayout {
    /**
     * A view used to render a shadow behind the sheet and extends outside the bounds of its parent
     * view.
     */
    public static class ShadowLayerView extends View {
        /** The length of the shadow in any direction. */
        private int mShadowLength;

        /**
         * Constructor to inflate from XML.
         *
         * @param context The Context the view is running in.
         * @param atts The attributes of the XML tag inflating the view.
         */
        public ShadowLayerView(Context context, @Nullable AttributeSet atts) {
            super(context, atts);
            Resources resources = context.getResources();
            setShadowLength(resources.getDimensionPixelSize(R.dimen.bottom_sheet_shadow_length));
        }

        /**
         * Sets the length of the shadow.
         *
         * @param length The length of the shadow in pixels.
         */
        public void setShadowLength(int length) {
            mShadowLength = length;
            setTranslationX((LocalizationUtils.isLayoutRtl() ? 1 : -1) * mShadowLength);
            setTranslationY(-mShadowLength);
            ViewUtils.requestLayout(this, "BottomSheetView.ShadowLayerView.setShadowLength");
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(widthMeasureSpec) + 2 * mShadowLength,
                            MeasureSpec.EXACTLY),
                    MeasureSpec.makeMeasureSpec(
                            MeasureSpec.getSize(heightMeasureSpec) + mShadowLength,
                            MeasureSpec.EXACTLY));
        }
    }

    /**
     * Constructor for inflating from XML.
     *
     * @param context The Context the view is running in.
     * @param atts The attributes of the XML tag inflating the view.
     */
    public BottomSheetView(Context context, @Nullable AttributeSet atts) {
        super(context, atts);
    }
}
