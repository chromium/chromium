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
 * consumer and initialize an {@link EdgeToEdgeController} when appropriate gesture navigation
 * insets are seen. This delayed creation is needed as window insets are not fully reliable during
 * initialization, and thus it is safer to wait until the right insets are explicitly seen, rather
 * than assume that the absence of tappable navigation insets are enough to guess that the device is
 * in gesture navigation.
 */
@NullMarked
public class EdgeToEdgeControllerCreator {
    private final InsetObserver.WindowInsetsConsumer mWindowInsetsConsumer;
    private final WeakReference<Activity> mActivity;
    private final Runnable mInitializeEdgeToEdgeController;

    private InsetObserver mInsetObserver;

    /**
     * Creates an EdgeToEdgeControllerCreator, which will listen to the InsetObserver as a window
     * inset consumer and will initialize an {@link EdgeToEdgeController} when appropriate gesture
     * navigation insets are seen.
     *
     * @param activity The current Activity, for evaluating if edge-to-edge is supported by the
     *     current configuration.
     * @param insetObserver The {@link InsetObserver} for observing window insets.
     * @param initializeEdgeToEdgeController The runnable to initialize the {@link
     *     EdgeToEdgeController} when the conditions are right.
     */
    public EdgeToEdgeControllerCreator(
            WeakReference<Activity> activity,
            InsetObserver insetObserver,
            Runnable initializeEdgeToEdgeController) {
        mActivity = activity;
        mInsetObserver = insetObserver;
        mInitializeEdgeToEdgeController = initializeEdgeToEdgeController;
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

        Insets navigationBarInsets = insets.getInsets(WindowInsetsCompat.Type.navigationBars());
        if (EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(activity)
                && EdgeToEdgeUtils.doAllInsetsIndicateGestureNavigation(insets)
                && !navigationBarInsets.equals(Insets.NONE)) {
            mInitializeEdgeToEdgeController.run();
        }
        return insets;
    }

    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
            mInsetObserver = null;
        }
    }
}
