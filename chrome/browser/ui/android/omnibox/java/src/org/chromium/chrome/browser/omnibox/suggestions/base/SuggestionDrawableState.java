// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.util.ObjectsCompat;

/** Represents graphical decoration for the suggestion components. */
public class SuggestionDrawableState {
    /** Embedded drawable object. */
    public final Drawable drawable;
    /** Whether supplied drawable can be tinted */
    public final boolean allowTint;
    /** Whether drawable should be rounded. */
    public final boolean useRoundedCorners;
    /** Whether drawable should be displayed as large. */
    public final boolean isLarge;
    /** The resource id for the icon to use. */
    // TODO(1092147): Remove this once robolectric shadows available in
    // chrome/android/native_java_unittests
    public final @DrawableRes int resourceId;

    /** Helper to construct SuggestionDrawableState objects.  */
    public static final class Builder {
        private Drawable mDrawable;
        private boolean mAllowTint;
        private boolean mUseRoundedCorners;
        private boolean mIsLarge;
        private @DrawableRes int mResourceId;

        /**
         * Create new Builder object.
         *
         * @param drawable Drawable object to use.
         */
        private Builder(Drawable drawable) {
            assert drawable != null : "SuggestionDrawableState needs a Drawable object";
            mDrawable = drawable;
        }

        /**
         * Associate Bitmap with built SuggestionDrawableState object.
         * @param ctx Current context.
         * @param bitmap Bitmap to use.
         */
        public static Builder forBitmap(Context ctx, Bitmap bitmap) {
            return new Builder(new BitmapDrawable(ctx.getResources(), bitmap));
        }

        /**
         * Associate Color with built SuggestionDrawableState object.
         *
         * @param color Color to use.
         */
        public static Builder forColor(@ColorInt int color) {
            return new Builder(new ColorDrawable(color));
        }

        /**
         * Associate Color with built SuggestionDrawableState object.
         *
         * @param ctx Current context.
         * @param colorRes Color resource to use.
         */
        public static Builder forColorRes(Context ctx, @ColorRes int colorRes) {
            return new Builder(new ColorDrawable(ctx.getColor(colorRes)));
        }

        /**
         * Associate Drawable with built SuggestionDrawableState object.
         *
         * @param ctx Current context.
         * @param res Drawable resource to use.
         */
        public static Builder forDrawableRes(Context ctx, @DrawableRes int res) {
            return new Builder(AppCompatResources.getDrawable(ctx, res)).setDrawableRes(res);
        }

        /**
         * Create new SuggestionDrawableState representing a supplied Drawable object.
         *
         * @param d Drawable object to use.
         */
        public static Builder forDrawable(Drawable d) {
            return new Builder(d);
        }

        /**
         * Specify whether built object should be rounded.
         *
         * @param useRoundedCorners true, if image should be rounded.
         */
        public Builder setUseRoundedCorners(boolean useRoundedCorners) {
            mUseRoundedCorners = useRoundedCorners;
            return this;
        }

        /**
         * Specify whether build object should receive tint.
         *
         * @param allowTint true, if built drawable state should be tinted to reflect theme.
         */
        public Builder setAllowTint(boolean allowTint) {
            mAllowTint = allowTint;
            return this;
        }

        /**
         * Specify whether build object should be presented as small (24dp) or large (36dp).
         *
         * @param isLarge true, if drawable should be shown large (36dp), otherwise 24dp.
         */
        public Builder setLarge(boolean isLarge) {
            mIsLarge = isLarge;
            return this;
        }

        /**
         * Specify Drawable resource.
         *
         * @param res Drawable resourc.
         */
        private Builder setDrawableRes(@DrawableRes int res) {
            mResourceId = res;
            return this;
        }

        /**
         * Build SuggestionDrawableState object.
         */
        public SuggestionDrawableState build() {
            return new SuggestionDrawableState(
                    mDrawable, mUseRoundedCorners, mIsLarge, mAllowTint, mResourceId);
        }
    }

    private SuggestionDrawableState(Drawable drawable, boolean useRoundedCorners, boolean isLarge,
            boolean allowTint, @DrawableRes int resId) {
        this.drawable = drawable;
        this.useRoundedCorners = useRoundedCorners;
        this.isLarge = isLarge;
        this.allowTint = allowTint;
        this.resourceId = resId;
    }

    @Override
    public boolean equals(Object object) {
        if (this == object) return true;
        if (!(object instanceof SuggestionDrawableState)) return false;
        SuggestionDrawableState other = (SuggestionDrawableState) object;

        return isLarge == other.isLarge && useRoundedCorners == other.useRoundedCorners
                && allowTint == other.allowTint && ObjectsCompat.equals(drawable, other.drawable);
    }
};
