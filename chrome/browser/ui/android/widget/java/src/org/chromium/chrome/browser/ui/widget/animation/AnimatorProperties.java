// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.animation;

import android.graphics.drawable.Drawable;
import android.util.Property;

/**
 * Holds different {@link Property} types that can be used with ObjectAnimators.
 */
public class AnimatorProperties {
    public static final Property<Drawable, Integer> DRAWABLE_ALPHA_PROPERTY =
            new Property<Drawable, Integer>(Integer.class, "alpha") {
                @Override
                public Integer get(Drawable d) {
                    // getAlpha() is only exposed on drawable in API 19+, so we rely on animations
                    // always setting the starting and ending values instead of relying on this
                    // property.
                    return 0;
                }

                @Override
                public void set(Drawable d, Integer alpha) {
                    d.setAlpha(alpha);
                }
            };
}