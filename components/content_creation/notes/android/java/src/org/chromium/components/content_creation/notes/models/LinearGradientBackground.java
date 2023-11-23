// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.ColorInt;

/** Model class for a template's background with a linear gradient. */
public final class LinearGradientBackground implements Background {
    public final @ColorInt int[] colors;
    public final LinearGradientDirection direction;

    /** Constructor. */
    public LinearGradientBackground(@ColorInt int[] colors, LinearGradientDirection direction) {
        this.colors = colors;
        this.direction = direction;
    }

    @Override
    public void apply(View view, float cornerRadius) {
        if (view == null) {
            return;
        }

        GradientDrawable drawable =
                new GradientDrawable(
                        LinearGradientDirection.toOrientation(this.direction), this.colors);
        drawable.setCornerRadius(cornerRadius);

        view.setBackground(drawable);
    }
}
