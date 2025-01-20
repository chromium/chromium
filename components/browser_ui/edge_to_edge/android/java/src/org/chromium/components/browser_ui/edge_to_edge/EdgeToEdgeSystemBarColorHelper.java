// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.graphics.Color;
import android.view.Window;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;

/**
 * Helper class that coordinates whether to apply the color changes to system window, or external
 * delegate based on the edge to edge status for the current activity window. When the window switch
 * to drawing edge to edge, the window's nav bar will be set to Color.TRANSPARENT.
 *
 * <p>This instance is meant to be created at the based activity level, and one instance per
 * activity. This class will use the window's bar color when it's initialized.
 */
public class EdgeToEdgeSystemBarColorHelper extends BaseSystemBarColorHelper {
    private final ObservableSupplier<Boolean> mDoesContentFitWindowSupplier;
    private final OneshotSupplier<SystemBarColorHelper> mEdgeToEdgeDelegateHelperSupplier;
    private final WindowSystemBarColorHelper mWindowColorHelper;
    private final Callback<Boolean> mOnEdgeToEdgeChanged = this::onContentFitsWindowChanged;

    protected boolean mIsActivityEdgeToEdge;

    /**
     * @param window Window from {@link android.app.Activity#getWindow()}.
     * @param doesContentFitWindowSupplier Supplier of whether the activity content fits the window
     *     insets.
     * @param delegateHelperSupplier Delegate helper that colors the bar when edge to edge.
     */
    public EdgeToEdgeSystemBarColorHelper(
            @NonNull Window window,
            @NonNull ObservableSupplier<Boolean> doesContentFitWindowSupplier,
            @NonNull OneshotSupplier<SystemBarColorHelper> delegateHelperSupplier) {
        mDoesContentFitWindowSupplier = doesContentFitWindowSupplier;
        mEdgeToEdgeDelegateHelperSupplier = delegateHelperSupplier;
        mWindowColorHelper = new WindowSystemBarColorHelper(window);

        // Initial values. By default, read the values from window.
        mIsActivityEdgeToEdge = Boolean.FALSE.equals(mDoesContentFitWindowSupplier.get());
        mStatusBarColor = mWindowColorHelper.getStatusBarColor();
        mNavBarColor = mWindowColorHelper.getNavigationBarColor();
        mNavBarDividerColor = mWindowColorHelper.getNavigationBarDividerColor();

        mDoesContentFitWindowSupplier.addObserver(mOnEdgeToEdgeChanged);
        mEdgeToEdgeDelegateHelperSupplier.onAvailable(this::onDelegateColorHelperChanged);
    }

    @Override
    public void destroy() {
        mDoesContentFitWindowSupplier.removeObserver(mOnEdgeToEdgeChanged);
        mWindowColorHelper.destroy();
    }

    @Override
    protected void applyStatusBarColor() {
        // Don't support color status bar yet. Delegate to the window directly.
        mWindowColorHelper.setStatusBarColor(mStatusBarColor);
    }

    @Override
    protected void applyNavBarColor() {
        updateColors();
    }

    @Override
    protected void applyNavigationBarDividerColor() {
        updateColors();
    }

    private void onContentFitsWindowChanged(Boolean contentFitsWindow) {
        boolean toEdge = Boolean.FALSE.equals(contentFitsWindow);
        if (mIsActivityEdgeToEdge != toEdge) {
            mIsActivityEdgeToEdge = toEdge;
            updateColors();
        }
    }

    private void onDelegateColorHelperChanged(@NonNull SystemBarColorHelper delegate) {
        updateColors();
    }

    private void updateColors() {
        int windowNavColor = mIsActivityEdgeToEdge ? Color.TRANSPARENT : mNavBarColor;
        int windowNavDividerColor = mIsActivityEdgeToEdge ? Color.TRANSPARENT : mNavBarDividerColor;
        mWindowColorHelper.setNavigationBarColor(windowNavColor);
        mWindowColorHelper.setNavigationBarDividerColor(windowNavDividerColor);
        // When setting a transparent navbar for drawing toEdge, the system navbar contrast
        // should not be enforced - otherwise, some devices will apply a scrim to the navbar.
        mWindowColorHelper.setNavigationBarContrastEnforced(!mIsActivityEdgeToEdge);

        SystemBarColorHelper delegateHelper = mEdgeToEdgeDelegateHelperSupplier.get();
        if (delegateHelper != null && mIsActivityEdgeToEdge) {
            delegateHelper.setNavigationBarColor(mNavBarColor);
            delegateHelper.setNavigationBarDividerColor(mNavBarDividerColor);
        }
    }

    WindowSystemBarColorHelper getWindowHelperForTesting() {
        return mWindowColorHelper;
    }

    SystemBarColorHelper getEdgeToEdgeDelegateHelperForTesting() {
        return mEdgeToEdgeDelegateHelperSupplier.get();
    }
}
