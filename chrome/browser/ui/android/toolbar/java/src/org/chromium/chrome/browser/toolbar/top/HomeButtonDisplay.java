// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeColorProvider.TintObserver;

/** Interface for managing the display and behavior of home button(s) in the toolbar. */
@NullMarked
public interface HomeButtonDisplay extends TintObserver {

    /** Returns the main view representing the home button or its container. */
    View getView();

    /** Sets the visibility of the home button(s) view. */
    void setVisibility(int visibility);

    /** Returns the current visibility of the home button(s) view. */
    int getVisibility();

    /** Returns The current foreground color of the home button(s). */
    @Nullable
    ColorStateList getForegroundColor();

    /**
     * Sets the background resource for the home button(s).
     *
     * @param resId The drawable resource ID.
     */
    void setBackgroundResource(@DrawableRes int resId);

    /** Returns The measured width of the home button(s) view. */
    int getMeasuredWidth();

    /**
     * Updates the state of the home button(s) based on various states. This method is responsible
     * for deciding internal visibility of buttons if it's a coordinator, or the overall visibility
     * of the single home button.
     *
     * @param toolbarVisualState The current {@link ToolbarPhone.VisualState} of the toolbar.
     * @param isHomeButtonEnabled Whether the home button is enabled.
     * @param isHomepageNonNtp Whether the current homepage is not the New Tab Page.
     * @param urlHasFocus True if the url bar has focus.
     */
    void updateState(
            @ToolbarPhone.VisualState int toolbarVisualState,
            boolean isHomeButtonEnabled,
            boolean isHomepageNonNtp,
            boolean urlHasFocus);

    /**
     * Sets the view to be traversed before this home button view in accessibility.
     *
     * @param viewId The ID of the view.
     */
    void setAccessibilityTraversalBefore(@IdRes int viewId);

    /**
     * Sets the translation Y of the home button(s) view.
     *
     * @param translationY The Y translation.
     */
    void setTranslationY(float translationY);

    /**
     * Sets the clickable state of the home button(s) view.
     *
     * @param clickable True if clickable.
     */
    void setClickable(boolean clickable);

    /**
     * Sets up a key listener for keyboard navigation.
     *
     * @param listener The listener to attach.
     */
    void setOnKeyListener(View.OnKeyListener listener);
}
