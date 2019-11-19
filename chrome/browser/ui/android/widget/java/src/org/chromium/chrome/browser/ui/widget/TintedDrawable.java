// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PorterDuff;
import android.graphics.drawable.BitmapDrawable;
import android.support.v7.content.res.AppCompatResources;

/**
 * Implementation of BitmapDrawable that allows to tint the color of the drawable for all
 * bitmap drawable states.
 */
public class TintedDrawable extends BitmapDrawable {
    /**
     * The set of colors that just be used for tinting this bitmap drawable.
     */
    protected ColorStateList mTint;

    public TintedDrawable(Context context, Bitmap bitmap) {
        super(context.getResources(), bitmap);
        mTint = AppCompatResources.getColorStateList(context, R.color.standard_mode_tint);
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean ret = updateTintColor();
        super.onStateChange(state);
        return ret;
    }

    @Override
    public boolean isStateful() {
        return true;
    }

    /**
     * Sets the tint color for the given Drawable for all button states.
     * @param tint The set of colors to use to color the ImageButton.
     */
    public void setTint(ColorStateList tint) {
        if (mTint == tint) return;
        mTint = tint;
        updateTintColor();
    }

    /**
     * Factory method for creating a {@link TintedDrawable} with a resource id.
     */
    public static TintedDrawable constructTintedDrawable(Context context, int drawableId) {
        Bitmap icon = BitmapFactory.decodeResource(context.getResources(), drawableId);
        return new TintedDrawable(context, icon);
    }

    /**
     * Factory method for creating a {@link TintedDrawable} with a resource id and specific tint.
     */
    public static TintedDrawable constructTintedDrawable(
            Context context, int drawableId, int tintColorId) {
        TintedDrawable drawable = constructTintedDrawable(context, drawableId);
        drawable.setTint(AppCompatResources.getColorStateList(context, tintColorId));
        return drawable;
    }

    private boolean updateTintColor() {
        if (mTint == null) return false;
        setColorFilter(mTint.getColorForState(getState(), 0), PorterDuff.Mode.SRC_IN);
        return true;
    }
}
