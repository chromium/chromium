// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.graphics.drawable.shapes.Shape;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatImageView;
import androidx.core.view.ViewCompat;

/**
 * A custom {@link ImageView} that is able to render bitmaps and colors with rounded off corners.
 * The corner radii should be set through attributes. E.g.
 *
 *   <org.chromium.components.browser_ui.widget.RoundedCornerImageView
 *      app:cornerRadiusTopStart="8dp"
 *      app:cornerRadiusTopEnd="8dp"
 *      app:cornerRadiusBottomStart="8dp"
 *      app:cornerRadiusBottomEnd="8dp"
 *      app:roundedfillColor="@android:color/white"/>
 *
 * Note : This does not properly handle padding. Padding will not be taken into account when rounded
 * corners are used.
 */
public class RoundedCornerImageView extends AppCompatImageView {
    private final RectF mTmpRect = new RectF();
    private final Matrix mTmpMatrix = new Matrix();

    private final Paint mRoundedBackgroundPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Paint mRoundedContentPaint;
    private final Matrix mScaleMatrix = new Matrix();
    private boolean mRoundCorners;
    // True, if constructor had a chance to run.
    // This is needed, because ImageView's constructor may trigger updates on our end
    // if certain attributes (eg. Drawable) are supplied via layout attributes.
    private final boolean mIsInitialized;

    private Shape mRoundedRectangle;
    private @ColorInt int mFillColor = Color.TRANSPARENT;

    public RoundedCornerImageView(Context context) {
        this(context, null, 0);
    }

    public RoundedCornerImageView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public RoundedCornerImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);

        // Set attribute indicating that all required objects are created.
        mIsInitialized = true;

        int radiusTopStart = 0;
        int radiusTopEnd = 0;
        int radiusBottomStart = 0;
        int radiusBottomEnd = 0;
        int color = Color.TRANSPARENT;

        if (attrs != null) {
            TypedArray a = getContext().obtainStyledAttributes(
                    attrs, R.styleable.RoundedCornerImageView, 0, 0);
            radiusTopStart = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusTopStart, 0);
            radiusTopEnd = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusTopEnd, 0);
            radiusBottomStart = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusBottomStart, 0);
            radiusBottomEnd = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusBottomEnd, 0);

            color = a.getColor(
                    R.styleable.RoundedCornerImageView_roundedfillColor, Color.TRANSPARENT);
            a.recycle();
        }

        setRoundedCorners(radiusTopStart, radiusTopEnd, radiusBottomStart, radiusBottomEnd);
        setRoundedFillColor(color);
        refreshState();
    }

    /**
     * Sets the rounded corner fill color to {@code color}.  This can be used to make sure the
     * rounded shape shows even if the actual content isn't full-bleed (e.g. icon with transparency
     * or too small to reach the edges).
     * @param color The color to use.  Setting to {@link Color#TRANSPARENT} will remove the color.
     */
    public void setRoundedFillColor(@ColorInt int color) {
        mFillColor = color;
        mRoundedBackgroundPaint.setColor(color);
        invalidate();
    }

    // ImageView implementation.
    @Override
    public void setImageDrawable(@Nullable Drawable drawable) {
        super.setImageDrawable(drawable);
        refreshState();
    }

    @Override
    public void setImageResource(int resId) {
        super.setImageResource(resId);
        refreshState();
    }

    @Override
    public void setImageBitmap(Bitmap bm) {
        super.setImageBitmap(bm);
        refreshState();
    }

    public void setRoundedCorners(int cornerRadiusTopStart, int cornerRadiusTopEnd,
            int cornerRadiusBottomStart, int cornerRadiusBottomEnd) {
        mRoundCorners = (cornerRadiusTopStart != 0 || cornerRadiusTopEnd != 0
                || cornerRadiusBottomStart != 0 || cornerRadiusBottomEnd != 0);
        if (!mRoundCorners) return;

        float[] radii;
        if (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_LTR) {
            radii = new float[] {cornerRadiusTopStart, cornerRadiusTopStart, cornerRadiusTopEnd,
                    cornerRadiusTopEnd, cornerRadiusBottomEnd, cornerRadiusBottomEnd,
                    cornerRadiusBottomStart, cornerRadiusBottomStart};
        } else {
            radii = new float[] {cornerRadiusTopEnd, cornerRadiusTopEnd, cornerRadiusTopStart,
                    cornerRadiusTopStart, cornerRadiusBottomStart, cornerRadiusBottomStart,
                    cornerRadiusBottomEnd, cornerRadiusBottomEnd};
        }

        mRoundedRectangle = new RoundRectShape(radii, null, null);
    }

    private void refreshState() {
        Drawable drawable = getDrawable();

        // Do not update state if we were invoked from the ImageView's constructor
        // (before we had the chance to initialize our own private data).
        if (!mIsInitialized) {
            return;
        }

        if (drawable instanceof ColorDrawable) {
            mRoundedBackgroundPaint.setColor(((ColorDrawable) getDrawable()).getColor());
            mRoundedContentPaint = null;
        } else if (drawable instanceof BitmapDrawable
                && ((BitmapDrawable) drawable).getBitmap() != null) {
            mRoundedBackgroundPaint.setColor(mFillColor);
            mRoundedContentPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

            Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();

            mRoundedContentPaint.setShader(
                    new BitmapShader(bitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP));
        } else {
            mRoundedBackgroundPaint.setColor(mFillColor);
            mRoundedContentPaint = null;
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (!mRoundCorners) {
            super.onDraw(canvas);
            return;
        }

        final int width = getWidth() - getPaddingLeft() - getPaddingRight();
        final int height = getHeight() - getPaddingTop() - getPaddingBottom();
        if (width <= 0 || height <= 0) return;

        mRoundedRectangle.resize(width, height);

        final int saveCount = canvas.save();
        try {
            canvas.translate(getPaddingLeft(), getPaddingTop());

            if (mRoundedBackgroundPaint.getColor() != Color.TRANSPARENT) {
                mRoundedRectangle.draw(canvas, mRoundedBackgroundPaint);
                // Note: RoundedBackgroundPaint is also used as ColorDrawable.
                if (getDrawable() instanceof ColorDrawable) {
                    return;
                }
            }

            if (mRoundedContentPaint == null) {
                canvas.restoreToCount(saveCount);
                super.onDraw(canvas);
                return;
            }

            Shader shader = mRoundedContentPaint.getShader();
            if (shader != null) {
                Drawable drawable = getDrawable();
                Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
                mTmpMatrix.set(getImageMatrix());
                mTmpMatrix.preScale((float) drawable.getIntrinsicWidth() / bitmap.getWidth(),
                        (float) drawable.getIntrinsicHeight() / bitmap.getHeight());

                shader.setLocalMatrix(mTmpMatrix);

                mTmpRect.set(0, 0, bitmap.getWidth(), bitmap.getHeight());
                mTmpMatrix.mapRect(mTmpRect);
                canvas.clipRect(mTmpRect);
            }

            mRoundedRectangle.draw(canvas, mRoundedContentPaint);
        } finally {
            canvas.restoreToCount(saveCount);
        }
    }
}
