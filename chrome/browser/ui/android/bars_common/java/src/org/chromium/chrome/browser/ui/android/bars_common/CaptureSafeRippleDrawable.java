// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.bars_common;

import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RippleDrawable;
import android.view.Gravity;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/**
 * A centered, fixed-size {@link RippleDrawable} for buttons with no resting background that skips
 * drawing when rendered to a non-hardware-accelerated {@link Canvas} (such as compositor bitmap
 * screenshots). This is currently only used for the bottom bar.
 */
@NullMarked
public class CaptureSafeRippleDrawable extends RippleDrawable {
    /**
     * Creates a centered, fixed-size {@link CaptureSafeRippleDrawable}.
     *
     * <p>Because {@code <ripple>} XML resources (such as {@code R.drawable.default_icon_background}
     * and {@code R.drawable.default_icon_background_baseline}) always inflate as a base {@link
     * RippleDrawable}, this constructor takes the inner state selector drawable (e.g., {@code
     * R.drawable.default_icon_background_selector}) and programmatically applies the same {@code
     * <item>} layer attributes declared in {@code default_icon_background.xml}.
     *
     * @param color The ripple color.
     * @param content The interactive state selector drawable (hover, focus, and pressed states).
     * @param size The width and height in pixels for the centered content layer.
     */
    public CaptureSafeRippleDrawable(ColorStateList color, Drawable content, @Px int size) {
        super(color, content, /* mask= */ null);
        // Replicate the <item> attributes from R.drawable.default_icon_background so the selector
        // acts as the bounded, centered content layer for both the state overlay and ripple mask.
        setId(/* index= */ 0, android.R.id.content);
        setLayerSize(/* index= */ 0, size, size);
        setLayerGravity(/* index= */ 0, Gravity.CENTER);
    }

    @Override
    public void draw(Canvas canvas) {
        // Compositor screenshots draw to a Bitmap-backed Canvas (isHardwareAccelerated() == false).
        // Skip super.draw(canvas) so temporary pressed/hovered/focused state overlays from the
        // content selector aren't baked into the static screenshot.
        // TODO(crbug.com/568025647): Explicitly annotate/track capture Canvases in CaptureUtils
        // instead of relying on !canvas.isHardwareAccelerated().
        if (!canvas.isHardwareAccelerated()) {
            return;
        }
        super.draw(canvas);
    }
}
