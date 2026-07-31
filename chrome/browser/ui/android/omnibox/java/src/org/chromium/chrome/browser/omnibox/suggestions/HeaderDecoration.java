// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Rect;
import android.text.TextPaint;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Px;
import androidx.collection.SimpleArrayMap;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Draws headers above groups of suggestions. */
@NullMarked
public class HeaderDecoration extends RecyclerView.ItemDecoration {
    private final int mHeaderHeight;
    private final TextPaint mTextPaint;
    private final TextPaint mTextPaintIncognito;
    private final SimpleArrayMap<String, CharSequence> mEllipsizedTitleCache =
            new SimpleArrayMap<>();
    private float mLastHeaderAvailableWidth;
    private @Px int mStartPadding;
    private boolean mIsIncognito;

    public HeaderDecoration(Context context) {
        mHeaderHeight =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);

        // Prepare text paints for drawing headers.
        TextView tv = new TextView(context);
        mTextPaint = createTextPaint(tv, false);
        mTextPaintIncognito = createTextPaint(tv, true);
    }

    /**
     * Sets the start padding for suggestion group headers.
     *
     * @param startPadding Start padding in pixels.
     */
    public void setHeaderStartPadding(@Px int startPadding) {
        mStartPadding = startPadding;
    }

    /**
     * Sets whether the header decoration is for an incognito window.
     *
     * @param isIncognito Whether the header decoration is for an incognito window.
     */
    public void setIsIncognito(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    @SuppressWarnings("SetTextColorAndSetTextSizeCheck")
    private TextPaint createTextPaint(TextView tv, boolean isIncognito) {
        tv.setTextAppearance(ChromeColors.getTextMediumThickSecondaryStyle(isIncognito));
        TextPaint paint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(tv.getCurrentTextColor());
        paint.setTextSize(tv.getTextSize());
        paint.setTypeface(tv.getTypeface());
        return paint;
    }

    @Override
    public void getItemOffsets(
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        PropertyModel model = getModel(view, parent);
        if (model != null
                && !TextUtils.isEmpty(model.get(SuggestionCommonProperties.HEADER_TITLE))) {
            outRect.top = mHeaderHeight;
        }
    }

    @Override
    public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        float headerAvailableWidth =
                Math.max(
                        0,
                        parent.getWidth()
                                - parent.getPaddingLeft()
                                - parent.getPaddingRight()
                                - mStartPadding);

        if (headerAvailableWidth != mLastHeaderAvailableWidth) {
            mEllipsizedTitleCache.clear();
            mLastHeaderAvailableWidth = headerAvailableWidth;
        }

        for (int i = 0; i < parent.getChildCount(); i++) {
            View child = parent.getChildAt(i);
            PropertyModel model = getModel(child, parent);
            if (model == null) continue;

            String title = model.get(SuggestionCommonProperties.HEADER_TITLE);
            if (TextUtils.isEmpty(title)) continue;

            TextPaint paint = mIsIncognito ? mTextPaintIncognito : mTextPaint;

            CharSequence ellipsizedTitle = mEllipsizedTitleCache.get(title);
            if (ellipsizedTitle == null) {
                ellipsizedTitle =
                        TextUtils.ellipsize(
                                title, paint, headerAvailableWidth, TextUtils.TruncateAt.END);
                mEllipsizedTitleCache.put(title, ellipsizedTitle);
            }

            float x = parent.getPaddingLeft() + mStartPadding;
            if (parent.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL) {
                paint.setTextAlign(Paint.Align.RIGHT);
                x = parent.getWidth() - parent.getPaddingRight() - mStartPadding;
            } else {
                paint.setTextAlign(Paint.Align.LEFT);
            }

            FontMetrics fm = paint.getFontMetrics();
            float y =
                    (child.getTop() + child.getTranslationY())
                            - (mHeaderHeight / 2f)
                            - ((fm.bottom + fm.top) / 2f);

            canvas.drawText(ellipsizedTitle.toString(), x, y, paint);
        }
    }

    private @Nullable PropertyModel getModel(View view, RecyclerView parent) {
        if (parent.getChildViewHolder(view)
                instanceof SimpleRecyclerViewAdapter.ViewHolder suggestionViewHolder) {
            return suggestionViewHolder.model;
        }
        return null;
    }
}
