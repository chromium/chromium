// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import android.app.Activity;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

import java.lang.ref.WeakReference;

/**
 * The EdgeToEdgeControllerCreator exists just to listen to the InsetObserver as a window inset
 * consumer and initialize an {@link EdgeToEdgeController} when appropriate window insets are seen.
 *
 * <p>When controller initialization is gated on bottom edge-to-edge (e.g., bottom chin and {@link
 * SimpleEdgeToEdgeController}), this delayed creation waits for gesture-navigation (non-tappable)
 * insets. Window insets are not fully reliable during initialization, so it is safer to wait until
 * gesture navigation insets are explicitly seen rather than assuming that the absence of tappable
 * navigation insets means the device is in gesture navigation mode.
 *
 * <p>When top edge-to-edge is supported by the host activity and device, {@link
 * EdgeToEdgeController} is initialized once non-empty status bar insets are seen, without waiting
 * for gesture navigation insets.
 */
@NullMarked
public class EdgeToEdgeControllerCreator {
    private final InsetObserver.WindowInsetsConsumer mWindowInsetsConsumer;
    private final WeakReference<Activity> mActivity;
    private final Runnable mInitializeEdgeToEdgeController;
    private final boolean mSupportsTopInset;

    private InsetObserver mInsetObserver;

    /**
     * Creates an EdgeToEdgeControllerCreator, which will listen to the InsetObserver as a window
     * inset consumer and will initialize an {@link EdgeToEdgeController} when appropriate window
     * insets are seen (e.g., gesture navigation insets for bottom edge-to-edge, or status bar
     * insets for top edge-to-edge).
     *
     * @param activity The current Activity, for evaluating if edge-to-edge is supported by the
     *     current configuration.
     * @param insetObserver The {@link InsetObserver} for observing window insets.
     * @param initializeEdgeToEdgeController The runnable to initialize the {@link
     *     EdgeToEdgeController} when the conditions are right.
     * @param supportsTopInset Whether top inset edge-to-edge is supported by the caller, so top
     *     edge-to-edge initialization only triggers for ChromeTabbedActivity and not secondary
     *     activities that use {@link SimpleEdgeToEdgeController}.
     */
    public EdgeToEdgeControllerCreator(
            WeakReference<Activity> activity,
            InsetObserver insetObserver,
            Runnable initializeEdgeToEdgeController,
            boolean supportsTopInset) {
        mActivity = activity;
        mInsetObserver = insetObserver;
        mInitializeEdgeToEdgeController = initializeEdgeToEdgeController;
        mSupportsTopInset = supportsTopInset;
        mWindowInsetsConsumer = this::onApplyWindowInsets;
        mInsetObserver.addInsetsConsumer(
                mWindowInsetsConsumer, InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_CREATOR);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    @VisibleForTesting
    WindowInsetsCompat onApplyWindowInsets(View view, WindowInsetsCompat insets) {
        if (mInsetObserver == null) return insets;
        if (mInsetObserver.hasInsetsConsumer(InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL)) {
            return insets;
        }
        @Nullable Activity activity = mActivity.get();
        if (activity == null) return insets;

        // Bottom chin requires gesture navigation insets, whereas top edge-to-edge (migrated
        // from TopInsetCoordinator) only requires status bar insets and operates independently
        // of the navigation bar mode.
        if (shouldInitializeForBottomEdgeToEdge(activity, insets)
                || shouldInitializeForTopEdgeToEdge(activity, insets)) {
            mInitializeEdgeToEdgeController.run();
        }
        return insets;
    }

    /**
     * Returns whether the {@link EdgeToEdgeController} should be initialized for bottom
     * edge-to-edge (e.g., bottom chin and {@link SimpleEdgeToEdgeController}), which requires
     * waiting for non-empty gesture-navigation (non-tappable) navigation bar insets.
     */
    private static boolean shouldInitializeForBottomEdgeToEdge(
            Activity activity, WindowInsetsCompat insets) {
        Insets navigationBarInsets = insets.getInsets(WindowInsetsCompat.Type.navigationBars());
        return EdgeToEdgeUtils.isEdgeToEdgeBottomChinSupportedByDevice(activity)
                && EdgeToEdgeUtils.doAllInsetsIndicateGestureNavigation(insets)
                && !navigationBarInsets.equals(Insets.NONE);
    }

    /**
     * Returns whether the {@link EdgeToEdgeController} should be initialized for top edge-to-edge,
     * which requires non-empty status bar insets and does not depend on the navigation bar mode.
     */
    private boolean shouldInitializeForTopEdgeToEdge(Activity activity, WindowInsetsCompat insets) {
        Insets statusBarsInsets = insets.getInsets(WindowInsetsCompat.Type.statusBars());
        return mSupportsTopInset
                && EdgeToEdgeUtils.isEdgelessTopInsetSupported(activity)
                && !statusBarsInsets.equals(Insets.NONE);
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
            mInsetObserver = null;
        }
    }
}
