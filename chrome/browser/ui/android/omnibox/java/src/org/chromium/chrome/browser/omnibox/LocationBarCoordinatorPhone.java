// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.animation.Animator;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;

import java.util.List;

/** A supplement to {@link LocationBarCoordinator} with methods specific to smaller devices. */
public class LocationBarCoordinatorPhone implements LocationBarCoordinator.SubCoordinator {
    private LocationBarPhone mLocationBarPhone;
    private StatusCoordinator mStatusCoordinator;

    public LocationBarCoordinatorPhone(
            LocationBarPhone phoneLayout, StatusCoordinator statusCoordinator) {
        mLocationBarPhone = phoneLayout;
        mStatusCoordinator = statusCoordinator;
    }

    @Override
    public void destroy() {
        mLocationBarPhone = null;
        mStatusCoordinator = null;
    }

    /**
     * Returns the total width of child views before the first view that would be visible when
     * location bar is focused. The first visible, focused view should be either url bar or status
     * icon.
     */
    public int getOffsetOfFirstVisibleFocusedView() {
        return mLocationBarPhone.getOffsetOfFirstVisibleFocusedView();
    }

    /**
     * Populates fade animator of status icon for location bar focus change animation.
     *
     * @param animators The target list to add animators to.
     * @param startDelayMs Start delay of fade animation in milliseconds.
     * @param durationMs Duration of fade animation in milliseconds.
     * @param targetAlpha Target alpha value.
     */
    public void populateFadeAnimation(
            List<Animator> animators, long startDelayMs, long durationMs, float targetAlpha) {
        mStatusCoordinator.populateFadeAnimation(animators, startDelayMs, durationMs, targetAlpha);
    }

    /**
     * Returns {@link FrameLayout.LayoutParams} of the LocationBar view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getLayoutParams()
     */
    public FrameLayout.LayoutParams getFrameLayoutParams() {
        return mLocationBarPhone.getFrameLayoutParams();
    }

    /**
     * The opacity of the view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getAlpha()
     */
    public float getAlpha() {
        return mLocationBarPhone.getAlpha();
    }

    /**
     * Bottom position of this view relative to its parent.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getBottom()
     * @return The bottom of this view, in pixels.
     */
    public int getBottom() {
        return mLocationBarPhone.getBottom();
    }

    /**
     * Returns the resolved layout direction for this view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getLayoutDirection()
     * @return {@link View#LAYOUT_DIRECTION_LTR}, or {@link View#LAYOUT_DIRECTION_RTL}.
     */
    public int getLayoutDirection() {
        return mLocationBarPhone.getLayoutDirection();
    }

    /**
     * Returns the end padding of this view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getPaddingEnd()
     * @return The end padding in pixels.
     */
    public int getPaddingEnd() {
        return mLocationBarPhone.getPaddingEnd();
    }

    /**
     * Returns the start padding of this view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getPaddingStart()
     * @return The start padding in pixels.
     */
    public int getPaddingStart() {
        return mLocationBarPhone.getPaddingStart();
    }

    /**
     * Top position of this view relative to its parent.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getTop()
     * @return The top of this view, in pixels.
     */
    public int getTop() {
        return mLocationBarPhone.getTop();
    }

    /**
     * The vertical location of this view relative to its top position, in pixels.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getTranslationY()
     */
    public float getTranslationY() {
        return mLocationBarPhone.getTranslationY();
    }

    /**
     * Returns the visibility status for this view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getVisibility()
     */
    public int getVisibility() {
        return mLocationBarPhone.getVisibility();
    }

    /**
     * Returns true if this view has focus itself, or is the ancestor of the view that has focus.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#hasFocus()
     */
    public boolean hasFocus() {
        return mLocationBarPhone.hasFocus();
    }

    /**
     * Invalidate the whole view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#invalidate()
     */
    public void invalidate() {
        mLocationBarPhone.invalidate();
    }

    /**
     * Sets the opacity of the view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#setAlpha(float)
     */
    public void setAlpha(float alpha) {
        mLocationBarPhone.setAlpha(alpha);
    }

    /**
     * Sets the padding.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#setPadding(int, int, int, int)
     */
    public void setPadding(int left, int top, int right, int bottom) {
        mLocationBarPhone.setPadding(left, top, right, bottom);
    }

    /**
     * Sets the horizontal location of this view relative to its left position.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#setTranslationX(float)
     */
    public void setTranslationX(float translationX) {
        mLocationBarPhone.setTranslationX(translationX);
    }

    /**
     * Sets the vertical location of this view relative to its top position.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#setTranslationY(float)
     */
    public void setTranslationY(float translationY) {
        mLocationBarPhone.setTranslationY(translationY);
    }

    /**
     * Returns the LocationBar view for use in drawing.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see ViewGroup#drawChild(Canvas, View, long)
     */
    public View getViewForDrawing() {
        return mLocationBarPhone;
    }
}
