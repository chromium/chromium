// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.content.Context;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.thinwebview.internal.ThinWebViewImpl;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;

/** Factory for creating a {@link ThinWebView}. */
@NullMarked
public class ThinWebViewFactory {

    @Nullable private static ThinWebView sInstanceForTesting;

    /**
     * Creates a {@link ThinWebView} backed by a {@link Surface}. The surface is provided by a
     * either a {@link TextureView} or {@link SurfaceView}.
     *
     * @param context The context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param intentRequestTracker A IntentRequestTracker to be used for the ThinWebView.
     * @param enablePermissionRequests Whether to enable permission requests.
     */
    public static ThinWebView create(
            Context context,
            ThinWebViewConstraints constraints,
            IntentRequestTracker intentRequestTracker,
            boolean enablePermissionRequests) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        return new ThinWebViewImpl(
                context, constraints, intentRequestTracker, enablePermissionRequests);
    }

    /**
     * Special method to create a {@link ThinWebView} using a specific {@link WindowAndroid}.
     *
     * <p>Most clients should NOT use this method as it might share a compositor with the existing
     * window. Typically, a ThinWebView requires an isolated rendering environment with its own
     * dedicated compositor. Reusing a {@link WindowAndroid} that is already associated with another
     * compositor (e.g. from the main Activity) might accidentally cause the original compositor to
     * be detached.
     *
     * @param context The context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param windowAndroid The {@link WindowAndroid} to be used for the ThinWebView.
     */
    public static ThinWebView create(
            Context context, ThinWebViewConstraints constraints, WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }

        return new ThinWebViewImpl(context, constraints, windowAndroid);
    }

    public static void setInstanceForTesting(ThinWebView thinWebView) {
        sInstanceForTesting = thinWebView;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
