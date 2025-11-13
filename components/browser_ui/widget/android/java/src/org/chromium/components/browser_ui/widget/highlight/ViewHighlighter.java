// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.highlight;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Px;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A helper class to draw an overlay layer on the top of a view to enable highlighting. The overlay
 * layer can be specified to be a circle or a rectangle.
 */
@NullMarked
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
    private static class NumberPulser implements PulseDrawable.PulseEndAuthority {
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

    /** Possible highlight shapes. */
    public enum HighlightShape {
        CIRCLE,
        RECTANGLE;
    }

    /** Params for highlight customization. */
    public static class HighlightParams {
        private final HighlightShape mShape;
        // If true, the highlight will respect the view's padding. If false, it will be
        // centered within view's bounding box.
        private boolean mBoundsRespectPadding;
        // Used if drawable should pulse only a given number of times.
        private int mNumPulses;
        // Only valid for HighlightShape.CIRCLE.
        // Used to customize the size of pulse if needed.
        private PulseDrawable.@Nullable Bounds mCircleRadius;
        // Only valid for HighlightShape.RECTANGLE The corner radius of rectangle in pixels. Used
        // to created a rounded rectangle.
        @Px private int mTopCornerRadius;
        @Px private int mBottomCornerRadius;

        // How far the highlight should extend past the bounds of the view.
        @Px private int mHighlightExtension;

        public HighlightParams(HighlightShape shape) {
            mShape = shape;
        }

        /** @return shape of the highlight */
        public HighlightShape getShape() {
            return mShape;
        }

        /**
         * @param respectPadding whether the highlight should respect the view's padding or be
         * centered in its bounding box
         */
        public void setBoundsRespectPadding(boolean respectPadding) {
            mBoundsRespectPadding = respectPadding;
        }

        /**
         * @return if true, the highlight will respect the view's padding. If false, it will be
         *         centered within view's bounding box
         */
        public boolean getBoundsRespectPadding() {
            return mBoundsRespectPadding;
        }

        /**
         * Only supported for {@code HighlightShape#CIRCLE}.
         * @param radius custom definition of the size of highlight's pulse
         */
        public void setCircleRadius(PulseDrawable.Bounds radius) {
            assert mShape == HighlightShape.CIRCLE;
            mCircleRadius = radius;
        }

        /** @return custom definition of the size of highlight's pulse or null of not set */
        public PulseDrawable.@Nullable Bounds getCircleRadius() {
            return mCircleRadius;
        }

        /**
         * Only supported for {@code HighlightShape#RECTANGLE}.
         *
         * @param radius value in pixels of a corner radius of a rounded rectangle highlight
         */
        public void setCornerRadius(@Px int radius) {
            assert mShape == HighlightShape.RECTANGLE;
            mTopCornerRadius = radius;
            mBottomCornerRadius = radius;
        }

        /**
         * Only supported for {@code HighlightShape#RECTANGLE}.
         *
         * @param radius value in pixels of a top corner radius of a rounded rectangle highlight
         */
        public void setTopCornerRadius(@Px int radius) {
            assert mShape == HighlightShape.RECTANGLE;
            mTopCornerRadius = radius;
        }

        /**
         * Only supported for {@code HighlightShape#RECTANGLE}.
         *
         * @param radius value in pixels of a bottom corner radius of a rounded rectangle highlight
         */
        public void setBottomCornerRadius(@Px int radius) {
            assert mShape == HighlightShape.RECTANGLE;
            mBottomCornerRadius = radius;
        }

        /**
         * @return value in pixels of a corner radius of a rounded rectangle highlight
         */
        public @Px int getCornerRadius() {
            assert mTopCornerRadius == mBottomCornerRadius
                    : "Top/Bottom have different corner radius";
            return mTopCornerRadius;
        }

        /**
         * @return value in pixels of a top corner radius of a rounded rectangle highlight
         */
        public @Px int getTopCornerRadius() {
            return mTopCornerRadius;
        }

        /**
         * @return value in pixels of a bottom corner radius of a rounded rectangle highlight
         */
        public @Px int getBottomCornerRadius() {
            return mBottomCornerRadius;
        }

        /**
         * @param highlightExtension How far the highlight should be extended past the bounds of the
         *     view.
         */
        public void setHighlightExtension(@Px int highlightExtension) {
            mHighlightExtension = highlightExtension;
        }

        /**
         * @return Value in pixels of how far the highlight should extend past the bounds of the
         *         view.
         */
        public @Px int getHighlightExtension() {
            return mHighlightExtension;
        }

        /** @param num set if drawable should pulse only a certain number of times */
        public void setNumPulses(int num) {
            assert num > 0;
            mNumPulses = num;
        }

        /** @return if > 0 drawable should pulse exactly this number of times */
        public int getNumPulses() {
            return mNumPulses;
        }
    }

    /**
     * Attach a custom PulseDrawable as a highlight layer over the view.
     *
     * <p>Will not highlight if the view is already highlighted.
     *
     * @param view The view to be highlighted.
     * @param pulseDrawable The highlight.
     * @param highlightExtension How far in pixels the highlight should be extended past the bounds
     *     of the view. 0 should be passed if there should be no extension.
     */
    private static void attachViewAsHighlight(
            View view, PulseDrawable pulseDrawable, int highlightExtension) {
        boolean highlighted =
                view.getTag(R.id.highlight_state) != null
                        ? (boolean) view.getTag(R.id.highlight_state)
                        : false;
        if (highlighted) return;

        view.setTag(R.id.highlight_state, true);
        // Store the highlight drawable.
        view.setTag(R.id.highlight_drawable, pulseDrawable);

        // ViewRectProvider can listen to any layout changes.
        ViewRectProvider viewRectProvider = new ViewRectProvider(view);
        RectProvider.Observer observer =
                new RectProvider.Observer() {
                    @Override
                    public void onRectChanged() {
                        Rect drawingRect = new Rect();
                        view.getDrawingRect(drawingRect);
                        pulseDrawable.setBounds(
                                drawingRect.left - highlightExtension,
                                drawingRect.top - highlightExtension,
                                drawingRect.right + highlightExtension,
                                drawingRect.bottom + highlightExtension);

                        // Avoid adding the same drawable as view overlay several times.
                        if (view.getTag(R.id.highlight_drawable_overlay_added) == null) {
                            // Pulse drawable is added as a separate view overlay layer.
                            view.getOverlay().add(pulseDrawable);
                            pulseDrawable.start();
                            view.setTag(R.id.highlight_drawable_overlay_added, true);
                        }
                    }

                    @Override
                    public void onRectHidden() {
                        view.getOverlay().remove(pulseDrawable);
                        // Reset the tag so that when onRectChanged() is called again the drawable
                        // is added again.
                        view.setTag(R.id.highlight_drawable_overlay_added, null);
                    }
                };

        viewRectProvider.startObserving(observer);
        observer.onRectChanged();

        view.setTag(R.id.highlight_view_rect_provider, viewRectProvider);
    }

    /**
     * Create a highlight layer over the view. Will not highlight if the view is already
     * highlighted.
     *
     * @param view The view to be highlighted.
     * @param params Definition of the highlight.
     */
    public static void turnOnHighlight(View view, HighlightParams params) {
        if (view == null) return;

        PulseDrawable drawable = null;
        if (params.getShape() == HighlightShape.CIRCLE) {
            drawable =
                    createCircle(
                            view,
                            params.getNumPulses(),
                            params.getBoundsRespectPadding(),
                            params.getCircleRadius());
        } else {
            int topRadius = params.getTopCornerRadius();
            int bottomRadius = params.getBottomCornerRadius();
            drawable =
                    createRectangle(
                            view,
                            params.getNumPulses(),
                            params.getBoundsRespectPadding(),
                            topRadius,
                            bottomRadius);
        }
        attachViewAsHighlight(view, drawable, params.getHighlightExtension());
    }

    /**
     * Turns off the highlight from the view. The original background of the view is restored.
     * @param view The associated view.
     */
    public static void turnOffHighlight(View view) {
        if (view == null) return;

        boolean highlighted =
                view.getTag(R.id.highlight_state) != null
                        ? (boolean) view.getTag(R.id.highlight_state)
                        : false;
        if (!highlighted) return;
        view.setTag(R.id.highlight_state, false);

        PulseDrawable pulseDrawable = (PulseDrawable) view.getTag(R.id.highlight_drawable);
        if (pulseDrawable != null) {
            view.getOverlay().remove(pulseDrawable);
            view.setTag(R.id.highlight_drawable, null);
            view.setTag(R.id.highlight_drawable_overlay_added, null);
        }

        // Stop observing the layout changes.
        ViewRectProvider viewRectProvider =
                (ViewRectProvider) view.getTag(R.id.highlight_view_rect_provider);
        if (viewRectProvider != null) {
            viewRectProvider.stopObserving();
            view.setTag(R.id.highlight_view_rect_provider, null);
        }
    }

    /** Helper method to create a circular drawable from the values of {@code HighlightParams}. */
    private static PulseDrawable createCircle(
            View view,
            int numPulses,
            boolean boundsRespectPadding,
            PulseDrawable.@Nullable Bounds circleRadius) {
        PulseDrawable drawable = null;
        Context context = view.getContext();
        PulseDrawable.PulseEndAuthority pulseEndAuthority =
                numPulses > 0 ? new NumberPulser(view, numPulses) : null;
        if (circleRadius != null) {
            drawable = PulseDrawable.createCustomCircle(context, circleRadius, pulseEndAuthority);
        } else {
            drawable = PulseDrawable.createCircle(context, pulseEndAuthority);
        }
        if (boundsRespectPadding) {
            drawable.setInset(
                    ViewCompat.getPaddingStart(view),
                    view.getPaddingTop(),
                    ViewCompat.getPaddingEnd(view),
                    view.getPaddingBottom());
        }
        return drawable;
    }

    /**
     * Helper method to create a rectangular drawable from the values of {@code HighlightParams}.
     */
    private static PulseDrawable createRectangle(
            View view,
            int numPulses,
            boolean boundsRespectPadding,
            @Px int topCornerRadius,
            @Px int bottomCornerRadius) {
        PulseDrawable drawable = null;
        Context context = view.getContext();

        if (numPulses != 0) {
            drawable =
                    PulseDrawable.createRoundedRectangle(
                            context,
                            topCornerRadius,
                            bottomCornerRadius,
                            new NumberPulser(view, numPulses));
        } else {
            drawable =
                    PulseDrawable.createRoundedRectangle(
                            context, topCornerRadius, bottomCornerRadius);
        }

        if (boundsRespectPadding) {
            drawable.setInset(
                    ViewCompat.getPaddingStart(view),
                    view.getPaddingTop(),
                    ViewCompat.getPaddingEnd(view),
                    view.getPaddingBottom());
        }
        return drawable;
    }
}
