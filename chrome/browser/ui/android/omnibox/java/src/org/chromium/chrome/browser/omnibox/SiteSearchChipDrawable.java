// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.text.TextDirectionHeuristics;
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
    private final Paint mTextPaint;
    private final Paint mSeparatorPaint;
    private final float mFullTextWidth;
    private final float mMaxFadeWidth;
    private @ColorInt int mTextColor;
    private boolean mIsTextRtl;

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
        mFullTextWidth = mTextPaint.measureText(mLabel);
        mMaxFadeWidth = mTextPaint.measureText("mmm");
        mIsTextRtl = TextDirectionHeuristics.FIRSTSTRONG_LTR.isRtl(mLabel, 0, mLabel.length());
        mTextPaint.setTextAlign(mIsTextRtl ? Paint.Align.RIGHT : Paint.Align.LEFT);

        updateColors(context, isIncognito);
    }

    /** Updates the text and separator colors based on the incognito state. */
    void setIncognitoColorsEnabled(Context context, boolean isIncognito) {
        updateColors(context, isIncognito);
        updateFadeShader(getBounds(), getLayoutDirection());
        invalidateSelf();
    }

    private void updateColors(Context context, boolean isIncognito) {
        mTextColor = IncognitoColors.getDefaultTextColorLink(context, isIncognito);
        @ColorInt
        int separatorColor =
                context.getColor(
                        isIncognito
                                ? R.color.site_search_chip_separator_color_incognito
                                : R.color.site_search_chip_separator_color);

        mTextPaint.setColor(mTextColor);
        mSeparatorPaint.setColor(separatorColor);
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();
        boolean isRtl = getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;

        // Center vertically.
        int textY = bounds.top + (bounds.height() - mTextHeight) / 2 - mTextTopOffset;
        // LTR: |<- mPadding ->| [Label] |<- mPadding ->|<- mSeparatorWidth ->|<- mPadding ->|
        // RTL: |<- mPadding ->|<- mSeparatorWidth ->|<- mPadding ->| [Label] |<- mPadding ->|
        int separatorLeft =
                isRtl ? bounds.left + mPadding : bounds.right - mPadding - mSeparatorWidth;
        int textLeft =
                isRtl ? bounds.left + mPadding * 2 + mSeparatorWidth : bounds.left + mPadding;
        int textRight =
                Math.max(textLeft, isRtl ? bounds.right - mPadding : separatorLeft - mPadding);

        canvas.save();
        canvas.clipRect(textLeft, bounds.top, textRight, bounds.bottom);
        canvas.drawText(mLabel, mIsTextRtl ? textRight : textLeft, textY, mTextPaint);
        canvas.restore();

        // Draw vertical divider line.
        int separatorTop = bounds.top + (bounds.height() - mTextHeight) / 2;
        int separatorBottom = separatorTop + mTextHeight;
        canvas.drawRect(
                separatorLeft,
                separatorTop,
                separatorLeft + mSeparatorWidth,
                separatorBottom,
                mSeparatorPaint);
    }

    private void updateFadeShader(Rect bounds, int layoutDirection) {
        boolean isRtl = layoutDirection == View.LAYOUT_DIRECTION_RTL;
        mIsTextRtl =
                (isRtl
                                ? TextDirectionHeuristics.FIRSTSTRONG_RTL
                                : TextDirectionHeuristics.FIRSTSTRONG_LTR)
                        .isRtl(mLabel, 0, mLabel.length());
        mTextPaint.setTextAlign(mIsTextRtl ? Paint.Align.RIGHT : Paint.Align.LEFT);
        int separatorLeft =
                isRtl ? bounds.left + mPadding : bounds.right - mPadding - mSeparatorWidth;
        int textLeft =
                isRtl ? bounds.left + mPadding * 2 + mSeparatorWidth : bounds.left + mPadding;
        int textRight =
                Math.max(textLeft, isRtl ? bounds.right - mPadding : separatorLeft - mPadding);
        float availableTextWidth = textRight - textLeft;

        if (mFullTextWidth > availableTextWidth && availableTextWidth > 0) {
            float fadeWidth = Math.min(mMaxFadeWidth, availableTextWidth / 3f);
            float fadeStart = mIsTextRtl ? textLeft + fadeWidth : textRight - fadeWidth;
            float fadeEnd = mIsTextRtl ? textLeft : textRight;
            mTextPaint.setShader(
                    new LinearGradient(
                            fadeStart,
                            0,
                            fadeEnd,
                            0,
                            mTextColor,
                            mTextColor & 0x00FFFFFF,
                            Shader.TileMode.CLAMP));
        } else {
            mTextPaint.setShader(null);
        }
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
        return (int) Math.ceil(mFullTextWidth) + mPadding * 3 + mSeparatorWidth;
    }

    @Override
    public int getIntrinsicHeight() {
        // Return a height that fits the text plus padding.
        return mTextHeight + mPadding;
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        super.onBoundsChange(bounds);
        updateFadeShader(bounds, getLayoutDirection());
    }

    @Override
    public boolean onLayoutDirectionChanged(int layoutDirection) {
        updateFadeShader(getBounds(), layoutDirection);
        invalidateSelf();
        return true;
    }

    /** Returns the Paint used to draw the chip text, for test verification. */
    /* package */ Paint getTextPaintForTesting() {
        return mTextPaint;
    }
}
