// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;

import java.lang.ref.WeakReference;

/** Tinted {@link Bitmap} get updated based on application environment.*/
public class CachedTintedBitmap {
    private final @DrawableRes int mDrawableId;
    private final @ColorRes int mColorId;
    private WeakReference<Bitmap> mPreviousBitmap;
    private @ColorInt int mPreviousTint;

    /**
     * Creates a {@link CachedTintedBitmap} object.
     *
     * @param drawableId  The drawable resource reference for the {@link CachedTintedBitmap}.
     * @param colorId The color resource reference for the {@link CachedTintedBitmap}.
     */
    public CachedTintedBitmap(int drawableId, int colorId) {
        mDrawableId = drawableId;
        mColorId = colorId;
    }

    /**
     * Update the information on the tinted {@link Bitmap} and return the bitmap itself.
     * @param context Used to load colors and resources.
     * @return The updated bitmap.
     */
    public Bitmap getBitmap(Context context) {
        Bitmap newBitmap = mPreviousBitmap == null ? null : mPreviousBitmap.get();
        final @ColorInt int tint = context.getColor(mColorId);
        if (newBitmap == null || mPreviousTint != tint) {
            final Resources resources = context.getResources();
            if (tint == Color.TRANSPARENT) {
                newBitmap = BitmapFactory.decodeResource(resources, mDrawableId);
            } else {
                // Apply color filter on a bitmap, which will cause some performance overhead, but
                // it is worth the APK space savings by avoiding adding another large asset for the
                // Bitmap in night mode. Not using vector drawable here because it is close to the
                // maximum recommended vector drawable size 200dpx200dp.
                BitmapFactory.Options options = new BitmapFactory.Options();
                options.inMutable = true;
                newBitmap = BitmapFactory.decodeResource(resources, mDrawableId, options);
                Paint paint = new Paint();
                paint.setColorFilter(new PorterDuffColorFilter(tint, PorterDuff.Mode.SRC_ATOP));
                Canvas canvas = new Canvas(newBitmap);
                canvas.drawBitmap(newBitmap, 0, 0, paint);
            }
            mPreviousBitmap = new WeakReference<>(newBitmap);
            mPreviousTint = tint;
        }
        return newBitmap;
    }

    WeakReference<Bitmap> getPreviousBitmapForTesting() {
        return mPreviousBitmap;
    }

    int getPreviousTintForTesting() {
        return mPreviousTint;
    }
}
