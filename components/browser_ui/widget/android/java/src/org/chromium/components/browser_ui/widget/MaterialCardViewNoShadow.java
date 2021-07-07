// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;

import com.google.android.material.card.MaterialCardView;

/**
 * Extension of {@link MaterialCardView} that set outline provider to null
 * in order to remove shadow.
 */
public class MaterialCardViewNoShadow extends MaterialCardView {
    /**
     * Constructs an instance of MaterialCardViewNoShadow,
     * which is an extension of {@link MaterialCardView} with shadow removed.
     */
    public MaterialCardViewNoShadow(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.materialCardViewStyle);
    }

    /**
     * Constructs an instance of MaterialCardViewNoShadow,
     * which is an extension of {@link MaterialCardView} with shadow removed.
     */
    public MaterialCardViewNoShadow(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        this.setOutlineProvider(null);
    }
}
