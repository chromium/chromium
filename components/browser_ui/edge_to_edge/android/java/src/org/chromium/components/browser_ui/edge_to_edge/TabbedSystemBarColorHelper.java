// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.edge_to_edge;

import android.graphics.Color;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.edge_to_edge.layout.EdgeToEdgeLayoutCoordinator;

/**
 * Helper class that coordinates applying navigation bar or navigation bar divider color changes to
 * the EdgeToEdgeBottomChinCoordinator and status bar color changes to the {@link
 * EdgeToEdgeLayoutCoordinator} for Edge to Edge.
 *
 * <p>This instance is meant to be created at the activity level in ChromeTabbedActivity, one per
 * instance.
 */
@NullMarked
public class TabbedSystemBarColorHelper extends BaseSystemBarColorHelper {
    private final EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private final OneshotSupplier<SystemBarColorHelper> mBottomChinDelegateHelperSupplier;

    /**
     * @param edgeToEdgeLayoutCoordinator Coordinator to color the status bar and navigation bar if
     *     needed.
     * @param bottomChinDelegateHelperSupplier Delegate helper that colors the navigation bar when
     *     drawing edge-to-edge.
     */
    public TabbedSystemBarColorHelper(
            EdgeToEdgeLayoutCoordinator edgeToEdgeLayoutCoordinator,
            OneshotSupplier<SystemBarColorHelper> bottomChinDelegateHelperSupplier) {
        mEdgeToEdgeLayoutCoordinator = edgeToEdgeLayoutCoordinator;
        mBottomChinDelegateHelperSupplier = bottomChinDelegateHelperSupplier;

        mBottomChinDelegateHelperSupplier.onAvailable(this::onBottomChinDelegateColorHelperChanged);
    }

    @Override
    public void destroy() {}

    @Override
    protected void applyStatusBarColor() {
        mEdgeToEdgeLayoutCoordinator.setStatusBarColor(mStatusBarColor);
    }

    @Override
    protected void applyNavBarColor() {
        updateNavBarColors();
    }

    @Override
    protected void applyNavigationBarDividerColor() {
        updateNavBarColors();
    }

    private void onBottomChinDelegateColorHelperChanged(SystemBarColorHelper delegate) {
        updateNavBarColors();
    }

    private void updateNavBarColors() {
        SystemBarColorHelper bottomChinDelegateHelper = mBottomChinDelegateHelperSupplier.get();
        if (bottomChinDelegateHelper != null) {
            bottomChinDelegateHelper.setNavigationBarColor(mNavBarColor);
            bottomChinDelegateHelper.setNavigationBarDividerColor(mNavBarDividerColor);
            mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(Color.TRANSPARENT);
            mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(Color.TRANSPARENT);
        } else {
            mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(mNavBarColor);
            mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(mNavBarDividerColor);
        }
    }
}
