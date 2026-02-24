// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;

/** A drawable that draws a site search chip. */
@NullMarked
public class SiteSearchChipDrawable extends Drawable {
    private final String mLabel;
    private final @Px int mPadding;
    private final @ColorInt int mBackgroundColor;
    private final @ColorInt int mTextColor;
    private final Paint mPaint;
    private final Paint mTextPaint;

    /**
     * @param context The context.
     * @param label The text to display in the chip.
     */
    public SiteSearchChipDrawable(Context context, String label) {
        mLabel = label;
        mPadding = OmniboxResourceProvider.getSideSpacing(context);

        mBackgroundColor = android.graphics.Color.TRANSPARENT;
        mTextColor =
                org.chromium.components.browser_ui.styles.SemanticColorUtils
                        .getDefaultTextColorLink(context);

        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mPaint.setColor(mBackgroundColor);

        mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setColor(mTextColor);
        mTextPaint.setTextSize(
                context.getResources().getDimension(R.dimen.location_bar_url_text_size));
    }

    @Override
    public void draw(Canvas canvas) {
        Rect bounds = getBounds();

        // Draw Text
        // Center vertically.
        Paint.FontMetricsInt fm = mTextPaint.getFontMetricsInt();
        int textHeight = fm.bottom - fm.top;
        int textY = bounds.top + (bounds.height() - textHeight) / 2 - fm.top;

        canvas.drawText(mLabel, bounds.left + mPadding, textY, mTextPaint);
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
        mTextPaint.setAlpha(alpha);
    }

    @Override
    public void setColorFilter(@Nullable ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
        mTextPaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public int getIntrinsicWidth() {
        return (int) mTextPaint.measureText(mLabel) + mPadding * 2;
    }

    @Override
    public int getIntrinsicHeight() {
        // Return a height that fits the text plus padding.
        Paint.FontMetricsInt fm = mTextPaint.getFontMetricsInt();
        return fm.bottom - fm.top + mPadding;
    }
}
