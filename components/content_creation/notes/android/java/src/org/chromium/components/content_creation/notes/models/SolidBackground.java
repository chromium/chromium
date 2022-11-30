// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.ColorInt;

/**
 * Model class for a template's solid-colored background.
 */
public final class SolidBackground implements Background {
    public final @ColorInt int color;

    /** Constructor. */
    public SolidBackground(@ColorInt int color) {
        this.color = color;
    }

    @Override
    public void apply(View view, float cornerRadius) {
        if (view == null) {
            return;
        }

        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(this.color);
        drawable.setCornerRadius(cornerRadius);

        view.setBackground(drawable);
    }
}