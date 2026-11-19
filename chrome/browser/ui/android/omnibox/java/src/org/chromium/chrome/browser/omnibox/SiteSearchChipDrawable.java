// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.IncognitoColors;

/** A drawable that draws a site search chip. */
@NullMarked
public class SiteSearchChipDrawable extends Drawable {
    private final String mLabel;
    private final @Px int mPadding;
    private final @Px int mSeparatorWidth;
    private final @Px int mTextHeight;
    private final @Px int mTextTopOffset;
    private final @Px int mTextWidth;
    private final Paint mTextPaint;
    private final Paint mSeparatorPaint;

    /**
     * @param context The context.
     * @param label The text to display in the chip.
     * @param isIncognito Whether incognito colors are enabled.
     */
    public SiteSearchChipDrawable(Context context, String label, boolean isIncognito) {
        Resources res = context.getResources();
        mLabel = label;
        mPadding = res.getDimensionPixelSize(R.dimen.omnibox_suggestion_side_spacing_smallest);
        mSeparatorWidth = res.getDimensionPixelSize(R.dimen.location_bar_status_separator_width);

        mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setTextSize(res.getDimension(R.dimen.location_bar_url_text_size));

        mSeparatorPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

        Paint.FontMetricsInt fm = mTextPaint.getFontMetricsInt();
        mTextHeight = fm.bottom - fm.top;
        mTextTopOffset = fm.top;
        mTextWidth = (int) mTextPaint.measureText(mLabel);

        updateColors(context, isIncognito);
    }

    /** Updates the text and separator colors based on the incognito state. */
    void setIncognitoColorsEnabled(Context context, boolean isIncognito) {
        updateColors(context, isIncognito);
        invalidateSelf();
    }

    private void updateColors(Context context, boolean isIncognito) {
        @ColorInt int textColor = IncognitoColors.getDefaultTextColorLink(context, isIncognito);
        @ColorInt
        int separatorColor =
                context.getColor(
                        isIncognito
                                ? R.color.site_search_chip_separator_color_incognito
                                : R.color.site_search_chip_separator_color);

        mTextPaint.setColor(textColor);
        mSeparatorPaint.setColor(separatorColor);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        boolean isRtl = getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;

        // Draw Text
        // Center vertically.
        int textY = bounds.top + (bounds.height() - mTextHeight) / 2 - mTextTopOffset;
        int textX = isRtl ? bounds.left + mPadding * 2 + mSeparatorWidth : bounds.left + mPadding;

        canvas.drawText(mLabel, textX, textY, mTextPaint);

        // Draw vertical divider line.
        // LTR: |<- mPadding ->| [Label] |<- mPadding ->|<- mSeparatorWidth ->|<- mPadding ->|
        // RTL: |<- mPadding ->|<- mSeparatorWidth ->|<- mPadding ->| [Label] |<- mPadding ->|
        int separatorLeft =
                isRtl ? bounds.left + mPadding : bounds.right - mPadding - mSeparatorWidth;
        int separatorTop = bounds.top + (bounds.height() - mTextHeight) / 2;
        int separatorBottom = separatorTop + mTextHeight;
        canvas.drawRect(
                separatorLeft,
                separatorTop,
                separatorLeft + mSeparatorWidth,
                separatorBottom,
                mSeparatorPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mTextPaint.setAlpha(alpha);
        mSeparatorPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mTextPaint.setColorFilter(colorFilter);
        mSeparatorPaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public int getIntrinsicWidth() {
        return mTextWidth + mPadding * 3 + mSeparatorWidth;
    }

    @Override
    public int getIntrinsicHeight() {
        // Return a height that fits the text plus padding.
        return mTextHeight + mPadding;
    }

    @Override
    public boolean onLayoutDirectionChanged(int layoutDirection) {
        invalidateSelf();
        return true;
    }
}
