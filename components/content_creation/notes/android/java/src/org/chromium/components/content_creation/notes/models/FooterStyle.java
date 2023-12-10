// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import android.graphics.BlendMode;
import android.graphics.BlendModeColorFilter;
import android.graphics.PorterDuff;
import android.os.Build;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;

/** Model class for a template's text style. */
public class FooterStyle {
    public final @ColorInt int textColor;
    public final @ColorInt int logoColor;

    /** Constructor. */
    public FooterStyle(@ColorInt int textColor, @ColorInt int logoColor) {
        this.textColor = textColor;
        this.logoColor = logoColor;
    }

    public void apply(TextView footerLinkView, TextView footerTitleView, ImageView iconView) {
        footerLinkView.setTextColor(textColor);
        footerTitleView.setTextColor(textColor);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            iconView.setColorFilter(new BlendModeColorFilter(logoColor, BlendMode.SRC_IN));
        } else {
            iconView.setColorFilter(logoColor, PorterDuff.Mode.SRC_IN);
        }
    }
}
