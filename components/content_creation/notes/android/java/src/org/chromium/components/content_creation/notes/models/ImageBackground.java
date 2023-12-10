// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.graphics.Bitmap;
import android.view.Gravity;
import android.view.View;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

/** Model class for a template's image background. */
public final class ImageBackground implements Background {
    public final String imageUrl;

    private Bitmap mBitmap;

    /** Constructor. */
    public ImageBackground(String imageUrl) {
        this.imageUrl = imageUrl;
    }

    public void setBitmap(Bitmap bitmap) {
        mBitmap = bitmap;
    }

    public boolean isBitmapEmpty() {
        return mBitmap == null;
    }

    @Override
    public void apply(View view, float cornerRadius) {
        // The image has to have been loaded before trying to apply this background.
        assert mBitmap != null;

        if (view == null) {
            return;
        }

        RoundedBitmapDrawable drawable =
                RoundedBitmapDrawableFactory.create(view.getContext().getResources(), mBitmap);
        drawable.setCornerRadius(cornerRadius);
        drawable.setGravity(Gravity.FILL);

        view.setBackground(drawable);
    }
}
