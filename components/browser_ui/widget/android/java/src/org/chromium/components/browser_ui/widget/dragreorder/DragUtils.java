// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.core.graphics.ColorUtils;

/** While multiple drag adapters exist, this class holds shared functionality. */
public class DragUtils {
    private static final int ANIMATION_DURATION_MS = 100;

    private static @ColorInt int getColorFromView(View view, @ColorInt int fallback) {
        Drawable bg = view.getBackground();
        return bg instanceof ColorDrawable ? ((ColorDrawable) bg).getColor() : fallback;
    }

    /**
     * Builds a drag animation on the given View. Assumes that when there is no drag, the background
     * is fully transparent and the elevation is 0.
     * @param isDragging Whether a drag is happening or not.
     * @param view The view who's background should be modified.
     * @param dragColor The end color when being dragged.
     * @param dragElevation The end elevation when being dragged.
     * @return An un-started animator.
     */
    public static Animator createViewDragAnimation(
            boolean isDragging, View view, @ColorInt int dragColor, float dragElevation) {
        // Use the same color just with full transparency. If we use Color.TRANSPARENT, it's
        // actually using 0s for rgb values, and will temporarily cause a dark color to be shown.
        final @ColorInt int transparentColor = ColorUtils.setAlphaComponent(dragColor, 0);

        // If the particular view has not been dragged yet, it will not have a drawable with a
        // color. Can safely assume its background was fully transparent. But once that has
        // happened, we can then read out the background color to make smooth transitions instead of
        // assuming the previous animation completed.
        final @ColorInt int startColor = getColorFromView(view, transparentColor);

        final @ColorInt int endColor = isDragging ? dragColor : transparentColor;
        ValueAnimator colorAnimator = ValueAnimator.ofArgb(startColor, endColor);
        colorAnimator.addUpdateListener(
                (anim) -> view.setBackgroundColor((int) anim.getAnimatedValue()));

        float startElevation = view.getTranslationZ();
        float endElevation = isDragging ? dragElevation : 0;
        ValueAnimator elevationAnimator = ValueAnimator.ofFloat(startElevation, endElevation);
        elevationAnimator.addUpdateListener(
                (anim) -> view.setTranslationZ((float) anim.getAnimatedValue()));

        // When multiple conflicting animators are playing on the same View, it seems like the last
        // one to attach wins. So it effectively does not matter. So don't bother tracking
        // outstanding animations to save resources.
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.setDuration(ANIMATION_DURATION_MS);
        animatorSet.play(elevationAnimator).with(colorAnimator);
        return animatorSet;
    }
}
