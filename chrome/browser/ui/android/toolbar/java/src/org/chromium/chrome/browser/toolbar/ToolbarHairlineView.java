// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;

/**
 * View for the bottom hairline of the Toolbar. This is meant for the top controls and should not be
 * used as a divider/hairline for any other views.
 */
@NullMarked
public class ToolbarHairlineView extends AppCompatImageView implements TopControlLayer {

    /**
     * Constructor that is called when inflating a hairline view.
     *
     * @param context the context the hairline is running in.
     * @param attrs the attributes of the XML tag that is inflating the hairline.
     */
    public ToolbarHairlineView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setImageResource(R.drawable.toolbar_hairline);
        setScaleType(ScaleType.FIT_XY);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.HAIRLINE;
    }

    @Override
    public int getTopControlHeight() {
        return getHeight();
    }

    @Override
    public @TopControlVisibility int getTopControlVisibility() {
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        return getVisibility() == View.VISIBLE
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }
}
