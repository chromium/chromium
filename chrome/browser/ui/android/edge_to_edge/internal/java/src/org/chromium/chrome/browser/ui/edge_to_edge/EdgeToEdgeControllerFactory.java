// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.edge_to_edge.SystemBarColorHelper;
import org.chromium.ui.insets.InsetObserver;

/**
 * Creates an {@link EdgeToEdgeController} used to control drawing using the Android Edge to Edge
 * Feature. This allows drawing under Android System Bars.
 */
@NullMarked
public class EdgeToEdgeControllerFactory {

    /**
     * Creates an {@link EdgeToEdgeController} instance using the given activity and {@link
     * ObservableSupplier} for a Tab.
     *
     * @param activity The Android {@link Activity} to allow drawing under System Bars.
     * @param windowAndroid The current {@link WindowAndroid} to allow drawing under System Bars.
     * @param tabObservableSupplier Supplies an {@Link Observer} that is notified whenever the Tab
     *     changes.
     * @param edgeToEdgeManager Provides the edge-to-edge state and allows for requests to draw
     *     edge-to-edge.
     * @param browserControlsStateProvider Provides the state of the BrowserControls so we can tell
     *     if the Toolbar is changing.
     * @param layoutManagerSupplier The supplier of {@link LayoutManager} for checking the active
     *     layout type.
     * @return An EdgeToEdgeController to control drawing under System Bars, or {@code null} if this
     *     version of Android does not support the APIs needed.
     */
    @NullUnmarked // create_stripped_java_factory.py does not support annotations in generics
    public static @Nullable EdgeToEdgeController create(
            Activity activity,
            WindowAndroid windowAndroid,
            ObservableSupplier<Tab> tabObservableSupplier,
            EdgeToEdgeManager edgeToEdgeManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager fullscreenManager) {
        if (Build.VERSION.SDK_INT < VERSION_CODES.R) return null;
        assert EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(activity);
        return new EdgeToEdgeControllerImpl(
                activity,
                windowAndroid,
                tabObservableSupplier,
                null,
                edgeToEdgeManager,
                browserControlsStateProvider,
                layoutManagerSupplier,
                fullscreenManager);
    }

    /**
     * Build the coordinator that manages the edge-to-edge bottom chin.
     *
     * @param androidView The Android view for the bottom chin.
     * @param keyboardVisibilityDelegate A {@link KeyboardVisibilityDelegate} for watching keyboard
     *     visibility events.
     * @param insetObserver The {@link InsetObserver} for checking IME insets.
     * @param layoutManager The {@link LayoutManager} for adding new scene overlays.
     * @param requestRenderRunnable Runnable that requests a re-render of the scene overlay.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for observing the edge-to-edge
     *     status and window bottom insets.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     * @param fullscreenManager The {@link FullscreenManager} for provide the fullscreen state.
     * @param defaultVisibility Whether the bottom chin is visible by default.
     */
    public static SystemBarColorHelper createBottomChin(
            View androidView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            InsetObserver insetObserver,
            LayoutManager layoutManager,
            Runnable requestRenderRunnable,
            EdgeToEdgeController edgeToEdgeController,
            BottomControlsStacker bottomControlsStacker,
            FullscreenManager fullscreenManager,
            boolean defaultVisibility) {
        assert EdgeToEdgeUtils.isBottomChinFeatureEnabled();
        return new EdgeToEdgeBottomChinCoordinator(
                androidView,
                keyboardVisibilityDelegate,
                insetObserver,
                layoutManager,
                requestRenderRunnable,
                edgeToEdgeController,
                bottomControlsStacker,
                fullscreenManager,
                defaultVisibility);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge. Note: this doesn't
     * account for browser controls.
     *
     * @param view The view to be adjusted.
     */
    public static EdgeToEdgePadAdjuster createForView(View view) {
        return new SimpleEdgeToEdgePadAdjuster(view, /* enableClipToPadding= */ true);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge, relying on the
     * EdgeToEdgeController for the bottom inset.
     *
     * @param view The view to be adjusted.
     * @param edgeToEdgeController The {@link EdgeToEdgeController} for providing the appropriate
     *     bottom inset.
     */
    public static EdgeToEdgePadAdjuster createForView(
            View view, EdgeToEdgeController edgeToEdgeController) {
        return new SimpleEdgeToEdgePadAdjuster(
                view, edgeToEdgeController, /* enableClipToPadding= */ true);
    }

    /**
     * Creates an adjuster for padding to the view to account for edge-to-edge, and observe the
     * supplier if edge to edge is enabled.
     *
     * @param view The view to be adjusted.
     * @param edgeToEdgeControllerSupplier Supplier to the {@link EdgeToEdgeController}.
     */
    public static EdgeToEdgePadAdjuster createForViewAndObserveSupplier(
            View view,
            @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        return new SimpleEdgeToEdgePadAdjuster(
                view, edgeToEdgeControllerSupplier, /* enableClipToPadding= */ true);
    }
}
