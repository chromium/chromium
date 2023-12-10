// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.content.Context;

import org.chromium.components.thinwebview.internal.ThinWebViewImpl;
import org.chromium.ui.base.IntentRequestTracker;

/** Factory for creating a {@link ThinWebView}. */
public class ThinWebViewFactory {
    /**
     * Creates a {@link ThinWebView} backed by a {@link Surface}. The surface is provided by
     * a either a {@link TextureView} or {@link SurfaceView}.
     * @param context The context to create this view.
     * @param constraints A set of constraints associated with this view.
     * @param intentRequestTracker A IntentRequestTracker to be used for the ThinWebView.
     */
    public static ThinWebView create(
            Context context,
            ThinWebViewConstraints constraints,
            IntentRequestTracker intentRequestTracker) {
        return new ThinWebViewImpl(context, constraints, intentRequestTracker);
    }
}
