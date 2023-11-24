// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.view.View;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.OnApplyWindowInsetsListener;

/** Wraps calls to the Android OS Edge To Edge APIs so we can easily instrument them. */
public interface EdgeToEdgeOSWrapper {
    /**
     * Wraps {@code WindowCompat#setDecorFitsSystemWindows}.
     * Sets whether the decor view should fit root-level content views for WindowInsetsCompat.
     * This determines whether the window decoration should fit within the System Windows (inset
     * away from System Bars rather than to the edge of the screen beneath System Bars.
     * @param window The Android {@link Window} that we're changing the decoration for.
     * @param decorFitsSystemWindows If set to {@code false}, the framework will not fit the content
     *         view to the insets and will just pass through the WindowInsetsCompat to the content
     *         view.
     */
    void setDecorFitsSystemWindows(@NonNull Window window, boolean decorFitsSystemWindows);

    /**
     * Wraps {@link View#setPadding(int, int, int, int)}.
     * Sets the padding for the given View.
     * The view may add on the space required to display the scrollbars, depending on the style and
     * visibility of the scrollbars. So the values returned from getPaddingLeft(), getPaddingTop(),
     * getPaddingRight() and getPaddingBottom() may be different from the values set in this call.
     * @param view The {@link View} to pad.
     * @param left int: the left padding in pixels
     * @param top int: the top padding in pixels
     * @param right int: the right padding in pixels
     * @param bottom int: the bottom padding in pixels
     */
    void setPadding(View view, int left, int top, int right, int bottom);

    /**
     * Wraps {@link Window#setNavigationBarColor(int color)}
     * Sets the color of the navigation bar to the given color.
     * For this to take effect, the window must be drawing
     * the system bar backgrounds with WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS
     * and WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION must not be set. If is not opaque,
     * consider setting View.SYSTEM_UI_FLAG_LAYOUT_STABLE and
     * View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION.
     * @param window The Android {@link Window} whose bottom Navigation Bar background color to set.
     * @param color int: The color to use for the Navigation Bar background.
     */
    void setNavigationBarColor(@NonNull Window window, int color);

    /**
     * Wraps {@code ViewCompat#setOnApplyWindowInsetsListener}.
     * Set an {@link OnApplyWindowInsetsListener} to take over the policy for applying window insets
     * to this view. This will only take effect on devices with API 21 or above.
     * @param view The {@link View} to listen to.
     * @param listener An {@link OnApplyWindowInsetsListener} to be called when the insets change
     *         for the {@link Window} that contains the view.
     */
    void setOnApplyWindowInsetsListener(
            @NonNull View view, @Nullable OnApplyWindowInsetsListener listener);
}
