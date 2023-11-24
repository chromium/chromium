// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;

/** RippleDrawable that does not allow sharing across views. Returns a constant state of null. */
public class UnsharableRippleDrawable extends RippleDrawable {
    private static final ColorStateList DUMMY_COLOR = ColorStateList.valueOf(0xFFFFFF);

    public UnsharableRippleDrawable(ColorStateList color, Drawable content, Drawable mask) {
        super(color, content, mask);
    }

    /**
     * Constructor used for drawable inflation. Ideally this would call `super()` which is package
     * private unfortunately. The public constructor's color parameter must not be null. The color
     * passed during inflation will be overridden by the color set in the xml. The color xml
     * attribute is required so passing any color here should not cause any unintended issues.
     */
    public UnsharableRippleDrawable() {
        this(DUMMY_COLOR, null, null);
    }

    @Override
    public ConstantState getConstantState() {
        // Returning null means that these drawables will not be able to be shared. This avoids
        // issues with RecyclerView when backgrounds are shared and as result don't always fully
        // cover items of different heights.
        return null;
    }
}
