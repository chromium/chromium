// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.view.View;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
public interface CompositorView {
    /**@return The android {@link View} representing this widget. */
    View getView();

    /** Should be called for cleanup when the CompositorView instance is no longer used. */
    void destroy();

    /** Request compositor view to render a frame. */
    void requestRender();

    /**
     * /**
     * Sets opacity for the view. {@link ThinWebViewConstraints#supportsOpacity} must be true for
     * using this method.
     */
    void setAlpha(float alpha);
}
