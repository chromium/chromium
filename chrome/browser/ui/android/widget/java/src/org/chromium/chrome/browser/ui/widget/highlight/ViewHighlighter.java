// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.highlight;

import static org.chromium.chrome.browser.ui.widget.highlight.PulseDrawable.createCircle;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ui.widget.R;

/**
 * A helper class to draw an overlay layer on the top of a view to enable highlighting. The overlay
 * layer can be specified to be a circle or a rectangle.
 */
public class ViewHighlighter {
    /**
     * Represents the delay between when menu anchor/toolbar handle/expand button was tapped and
     * when the menu or bottom sheet opened up. Unfortunately this is sensitive because if it is too
     * small, we might clear the state before the menu item got a chance to be highlighted. If it is
     * too large, user might tap somewhere else and then open the menu/bottom sheet to see the UI
     * still highlighted.
     */
    public static final int IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS = 200;

    /**
     * Allows its associated PulseDrawable to pulse a specified number of times, then turns off the
     * PulseDrawable highlight.
     */
    public static class NumberPulser implements PulseDrawable.PulseEndAuthority {
        private final View mView;
        private int mNumPulsesRemaining;

        NumberPulser(View view, int numPulses) {
            mView = view;
            mNumPulsesRemaining = numPulses;
        }

        @Override
        public boolean canPulseAgain() {
            mNumPulsesRemaining--;
            if (mNumPulsesRemaining == 0) ViewHighlighter.turnOffHighlight(mView);
            return mNumPulsesRemaining > 0;
        }
    }

    public static void pulseHighlight(View view, boolean circular, int numPulses) {
        if (view == null) return;

        PulseDrawable pulseDrawable = circular
                ? createCircle(view.getContext(), new NumberPulser(view, numPulses))
                : PulseDrawable.createHighlight(
                        view.getContext(), new NumberPulser(view, numPulses));

        attachViewAsHighlight(view, pulseDrawable);
    }

    /**
     * Create a highlight layer over the view.
     * @param view The view to be highlighted.
     * @param circular Whether the highlight should be a circle or rectangle.
     */
    public static void turnOnHighlight(View view, boolean circular) {
        if (view == null) return;

        PulseDrawable pulseDrawable = circular ? PulseDrawable.createCircle(view.getContext())
                                               : PulseDrawable.createHighlight(view.getContext());

        attachViewAsHighlight(view, pulseDrawable);
    }

    /**
     * Attach a custom PulseDrawable as a highlight layer over the view.
     *
     * Will not highlight if the view is already highlighted.
     *
     * @param view The view to be highlighted.
     * @param pulseDrawable The highlight.
     */
    public static void attachViewAsHighlight(View view, PulseDrawable pulseDrawable) {
        boolean highlighted = view.getTag(R.id.highlight_state) != null
                ? (boolean) view.getTag(R.id.highlight_state)
                : false;
        if (highlighted) return;

        Resources resources = view.getContext().getResources();
        Drawable background = view.getBackground();
        if (background != null) {
            background = background.getConstantState().newDrawable(resources);
        }

        LayerDrawable drawable = ApiCompatibilityUtils.createLayerDrawable(background == null
                        ? new Drawable[] {pulseDrawable}
                        : new Drawable[] {background, pulseDrawable});
        view.setBackground(drawable);
        view.setTag(R.id.highlight_state, true);

        pulseDrawable.start();
    }

    /**
     * Turns off the highlight from the view. The original background of the view is restored.
     * @param view The associated view.
     */
    public static void turnOffHighlight(View view) {
        if (view == null) return;

        boolean highlighted = view.getTag(R.id.highlight_state) != null
                ? (boolean) view.getTag(R.id.highlight_state)
                : false;
        if (!highlighted) return;
        view.setTag(R.id.highlight_state, false);

        Resources resources = ContextUtils.getApplicationContext().getResources();
        Drawable existingBackground = view.getBackground();
        if (existingBackground instanceof LayerDrawable) {
            LayerDrawable layerDrawable = (LayerDrawable) existingBackground;
            if (layerDrawable.getNumberOfLayers() >= 2) {
                view.setBackground(
                        layerDrawable.getDrawable(0).getConstantState().newDrawable(resources));
            } else {
                view.setBackground(null);
            }
        }
    }
}