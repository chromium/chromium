// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;

/** A class representing one data row in the dialog. */
public class DeviceItemRow {
    public final String mKey;
    public String mDescription;
    public Drawable mIcon;
    public String mIconDescription;

    /**
     * Creates a device item row which can be shown in the dialog.
     *
     * @param key Item unique identifier.
     * @param description Item description.
     * @param icon Item icon.
     * @param iconDescription Item icon description.
     */
    public DeviceItemRow(
            String key,
            String description,
            @Nullable Drawable icon,
            @Nullable String iconDescription) {
        mKey = key;
        mDescription = description;
        mIcon = icon;
        mIconDescription = iconDescription;
    }

    /**
     * Returns true if all parameters match the corresponding member.
     *
     * @param key Expected item unique identifier.
     * @param description Expected item description.
     * @param icon Expected item icon.
     */
    public boolean hasSameContents(
            String key,
            String description,
            @Nullable Drawable icon,
            @Nullable String iconDescription) {
        if (!TextUtils.equals(mKey, key)) return false;
        if (!TextUtils.equals(mDescription, description)) return false;
        if (!TextUtils.equals(mIconDescription, iconDescription)) return false;

        if (icon != null && mIcon != null) {
            Drawable myIcon = mIcon.getConstantState().newDrawable();
            Drawable theirIcon = icon.getConstantState().newDrawable();

            Bitmap myBitmap =
                    Bitmap.createBitmap(
                            myIcon.getIntrinsicWidth(),
                            myIcon.getIntrinsicHeight(),
                            Bitmap.Config.ARGB_8888);
            Canvas myCanvas = new Canvas();
            myCanvas.setBitmap(myBitmap);
            myIcon.setBounds(0, 0, myCanvas.getWidth(), myCanvas.getHeight());
            myIcon.draw(myCanvas);

            Bitmap theirBitmap =
                    Bitmap.createBitmap(
                            theirIcon.getIntrinsicWidth(),
                            theirIcon.getIntrinsicHeight(),
                            Bitmap.Config.ARGB_8888);
            Canvas theirCanvas = new Canvas();
            theirCanvas.setBitmap(theirBitmap);
            theirIcon.setBounds(0, 0, theirCanvas.getWidth(), theirCanvas.getHeight());
            theirIcon.draw(theirCanvas);

            return myBitmap.sameAs(theirBitmap);
        }

        return icon == null && mIcon == null;
    }
}
