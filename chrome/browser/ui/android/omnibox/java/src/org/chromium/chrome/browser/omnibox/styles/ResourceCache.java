// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.styles;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Drawable.ConstantState;
import android.util.SparseArray;
import android.util.SparseIntArray;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Utility class to cache resolved resources (strings, dimensions, colors, drawables) to avoid
 * repeated Android framework lookups.
 */
@NullMarked
class ResourceCache {
    private static final String TAG = "ResourceCache";

    private final Context mContext;
    private final SparseArray<String> mStrings = new SparseArray<>();
    private final SparseIntArray mInts = new SparseIntArray();
    private final SparseArray<ColorStateList> mColorStateLists = new SparseArray<>();
    private final SparseArray<ConstantState> mDrawables = new SparseArray<>();

    ResourceCache(Context context) {
        mContext = context;
    }

    /** Resolves a string resource, replaces $ placeholder arguments, and caches the result. */
    String getString(@StringRes int resId) {
        ThreadUtils.assertOnUiThread();
        String string = mStrings.get(resId);
        if (string == null) {
            string = mContext.getString(resId);
            string = string.replaceAll("\\$(\\d+)", "%$1\\$s");
            mStrings.put(resId, string);
        }
        return string;
    }

    /** Resolves a dimension size and caches the result. */
    @Px
    int getDimen(@DimenRes int resId) {
        ThreadUtils.assertOnUiThread();
        int index = mInts.indexOfKey(resId);
        if (index >= 0) {
            return mInts.valueAt(index);
        }
        int size = mContext.getResources().getDimensionPixelSize(resId);
        mInts.put(resId, size);
        return size;
    }

    /** Resolves a color resource and caches the result. */
    @ColorInt
    int getColor(@ColorRes int resId) {
        ThreadUtils.assertOnUiThread();
        int index = mInts.indexOfKey(resId);
        if (index >= 0) {
            return mInts.valueAt(index);
        }
        @ColorInt int color = mContext.getColor(resId);
        mInts.put(resId, color);
        return color;
    }

    /**
     * Resolves a color from a theme attribute (e.g., R.attr.colorOnSurface) and caches the result.
     */
    @ColorInt
    int getColorAttr(@AttrRes int attrId) {
        ThreadUtils.assertOnUiThread();
        int index = mInts.indexOfKey(attrId);
        if (index >= 0) {
            return mInts.valueAt(index);
        }
        @ColorInt int color = MaterialColors.getColor(mContext, attrId, TAG);
        mInts.put(attrId, color);
        return color;
    }

    /** Resolves a ColorStateList resource and caches the result. */
    ColorStateList getColorStateList(@ColorRes int resId) {
        ThreadUtils.assertOnUiThread();
        ColorStateList list = mColorStateLists.get(resId);
        if (list == null) {
            list = AppCompatResources.getColorStateList(mContext, resId);
            mColorStateLists.put(resId, list);
        }
        return list;
    }

    /**
     * Resolves a drawable resource and caches its constant state. Subsequent fetches create new
     * Drawable instances using the cached constant state.
     */
    Drawable getDrawable(@DrawableRes int resId) {
        ThreadUtils.assertOnUiThread();
        ConstantState state = mDrawables.get(resId);
        if (state != null) {
            return state.newDrawable(mContext.getResources());
        }
        Drawable drawable = AppCompatResources.getDrawable(mContext, resId);
        mDrawables.put(resId, drawable.getConstantState());
        return drawable;
    }
}
