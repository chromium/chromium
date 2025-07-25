// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.NullMarked;

/**
 * A generic OnHoverListener that manually toggles the hover state on a view. This is useful for
 * child views that can't be clickable because the parent handles the clicks for them.
 */
@NullMarked
public class HoverHighlightViewListener implements View.OnHoverListener {
    /**
     * A generic OnHoverListener that manually toggles the hover state on a view. This is useful for
     * child views that can't be clickable because the parent handles the clicks for them.
     */
    public HoverHighlightViewListener() {}

    @Override
    public boolean onHover(@NonNull View view, @NonNull MotionEvent motionEvent) {
        switch (motionEvent.getAction()) {
            case MotionEvent.ACTION_HOVER_ENTER:
                view.setHovered(true);
                return false;
            case MotionEvent.ACTION_HOVER_EXIT:
                view.setHovered(false);
                return false;
            default:
                return false;
        }
    }
}
