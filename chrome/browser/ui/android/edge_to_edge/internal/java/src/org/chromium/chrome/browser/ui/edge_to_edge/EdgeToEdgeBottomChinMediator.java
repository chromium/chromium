// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.COLOR;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.HEIGHT;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeBottomChinProperties.Y_OFFSET;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils.isBottomChinAllowed;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.ui.modelutil.PropertyModel;

class EdgeToEdgeBottomChinMediator
        implements LayoutStateProvider.LayoutStateObserver,
                EdgeToEdgeSupplier.ChangeObserver,
                NavigationBarColorProvider.Observer {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    private @LayoutType int mCurrentLayoutType;
    private int mEdgeToEdgeBottomInset;

    private final @NonNull LayoutManager mLayoutManager;
    private final @NonNull EdgeToEdgeController mEdgeToEdgeController;
    private final @NonNull NavigationBarColorProvider mNavigationBarColorProvider;
    private final @NonNull BottomControlsStacker mBottomControlsStacker;

    /**
     * Build a new mediator for the bottom chin component.
     *
     * @param model The {@link EdgeToEdgeBottomChinProperties} that holds all the view state for the
     *     bottom chin component.
     * @param layoutManager The {@link LayoutManager} for observing active layout type.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param navigationBarColorProvider The {@link NavigationBarColorProvider} for observing the
     *     color for the navigation bar.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     */
    EdgeToEdgeBottomChinMediator(
            PropertyModel model,
            @NonNull LayoutManager layoutManager,
            @NonNull EdgeToEdgeController edgeToEdgeController,
            @NonNull NavigationBarColorProvider navigationBarColorProvider,
            @NonNull BottomControlsStacker bottomControlsStacker) {
        mModel = model;
        mLayoutManager = layoutManager;
        mEdgeToEdgeController = edgeToEdgeController;
        mNavigationBarColorProvider = navigationBarColorProvider;
        mBottomControlsStacker = bottomControlsStacker;

        mModel.set(Y_OFFSET, 0);

        mLayoutManager.addObserver(this);
        mEdgeToEdgeController.registerObserver(this);
        mNavigationBarColorProvider.addObserver(this);
    }

    void destroy() {
        assert mLayoutManager != null;
        assert mEdgeToEdgeController != null;
        assert mNavigationBarColorProvider != null;

        mLayoutManager.removeObserver(this);
        mEdgeToEdgeController.unregisterObserver(this);
        mNavigationBarColorProvider.removeObserver(this);
    }

    private void updateVisibility() {
        // TODO(crbug.com/350754745) Check if other bottom browser controls are showing
        // TODO add check for E2E website opt-in

        mModel.set(IS_VISIBLE, isBottomChinAllowed(mCurrentLayoutType, mEdgeToEdgeBottomInset));
    }

    // LayoutStateProvider.LayoutStateObserver

    @Override
    public void onStartedShowing(int layoutType) {
        mCurrentLayoutType = layoutType;
        updateVisibility();
    }

    // EdgeToEdgeSupplier.ChangeObserver

    @Override
    public void onToEdgeChange(int bottomInset) {
        mEdgeToEdgeBottomInset = bottomInset;
        mModel.set(HEIGHT, bottomInset);
        updateVisibility();
    }

    @Override
    public void onNavigationBarColorChanged(int color) {
        // TODO(): Animate the color change.
        mModel.set(COLOR, color);
    }
}
