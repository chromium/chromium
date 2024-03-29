// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Outline;
import android.graphics.Path;
import android.graphics.Path.Direction;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.AnimatedVectorDrawable;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewOutlineProvider;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatImageView;
import androidx.core.content.res.ResourcesCompat;

/**
 * ImageView with a scrolling gradient background and an outline with multiple horizontal lines,
 * meant to indicate a paragraph of text that's loading in an indeterminate state. For use in
 * Android API R+.
 *
 * <p>Example:
 *
 * <pre>{@code
 * <GradientParagraphLoadingView
 *     android:layout_width="match_parent"
 *     android:layout_height="55dp"
 *     app:numberOfLines="4"
 *     app:lineHeight="10dp"
 *     app:lineSpacing="5dp"
 *     app:lastLineWidthFraction="25%"/>
 * }</pre>
 *
 * This would draw 4 lines of 10dp height each with 5dp of space between each other, with the last
 * line having a width of 25% of this view. In Android APIs previous to R it will ignore
 * app:numberOfLines and show a single line.
 */
public class GradientParagraphLoadingView extends AppCompatImageView {

    private final int mLineCount;
    private final float mLineHeight;
    private final float mLineSpacing;
    private final float mLastLineWidthFraction;

    public GradientParagraphLoadingView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        TypedArray a =
                context.obtainStyledAttributes(attrs, R.styleable.GradientParagraphLoadingView);

        // Number of lines to draw on the outline.
        mLineCount =
                a.getInt(
                        R.styleable.GradientParagraphLoadingView_numberOfLines,
                        getResources().getInteger(R.integer.loading_text_line_count));
        // Height of each line drawn on the outline.
        mLineHeight =
                a.getDimension(
                        R.styleable.GradientParagraphLoadingView_lineHeight,
                        getResources().getDimension(R.dimen.loading_text_line_height));
        // Amount of space between lines.
        mLineSpacing =
                a.getDimension(
                        R.styleable.GradientParagraphLoadingView_lineSpacing,
                        getResources().getDimension(R.dimen.loading_text_line_spacing));
        // Width of the last line as a fraction of the width of this view. All previous lines are as
        // wide as this view.
        mLastLineWidthFraction =
                a.getFraction(
                        R.styleable.GradientParagraphLoadingView_lastLineWidthFraction,
                        1,
                        1,
                        getResources()
                                .getFraction(
                                        R.fraction.loading_text_last_line_width_fraction, 1, 1));

        assert mLineCount > 0 : "Line count must be a positive value";

        a.recycle();
        initialize(context);
    }

    private void initialize(Context context) {

        // TODO(salg): Handle theming on incognito mode.
        AnimatedVectorDrawable animatedGradientDrawable =
                (AnimatedVectorDrawable)
                        ResourcesCompat.getDrawable(
                                context.getResources(),
                                R.drawable.gradient_paragraph_loading_animation,
                                context.getTheme());
        setScaleType(ScaleType.CENTER_CROP);
        setImageDrawable(animatedGradientDrawable);

        setOutlineProvider(
                new ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, Outline outline) {
                        int viewWidth = view.getWidth();
                        if (viewWidth == 0 || view.getHeight() == 0) return;

                        float cornerRadius = mLineHeight / 2;

                        // Custom outline shapes aren't supported on Android API < R. Fallback to a
                        // single line.
                        if (VERSION.SDK_INT < VERSION_CODES.R) {
                            Rect fallbackRoundRect = new Rect(0, 0, viewWidth, (int) mLineHeight);
                            outline.setRoundRect(fallbackRoundRect, cornerRadius);
                            return;
                        }

                        // Draw all lines into a Path to use as an outline.
                        Path clipPath = new Path();
                        RectF textLineRect = new RectF(0, 0, viewWidth, mLineHeight);
                        for (int line = 0; line < mLineCount; line++) {
                            textLineRect.offsetTo(0, line * (mLineHeight + mLineSpacing));
                            if (line == mLineCount - 1) {
                                // On the last line change its width to a fraction.
                                textLineRect.right = (float) (viewWidth * mLastLineWidthFraction);
                            }
                            clipPath.addRoundRect(
                                    textLineRect, cornerRadius, cornerRadius, Direction.CW);
                        }

                        outline.setPath(clipPath);
                    }
                });
        setClipToOutline(true);

        animatedGradientDrawable.start();
    }
}
