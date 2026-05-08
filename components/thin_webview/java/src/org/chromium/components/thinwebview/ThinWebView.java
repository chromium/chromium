// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
@NullMarked
public interface ThinWebView {
    /** Returns the android {@link View} representing this widget. */
    View getView();

    /**
     * Method to be called to display the contents of a {@link WebContents} on the surface. The user
     * interactability is provided through the {@code contentView}.
     *
     * @param webContents A {@link WebContents} for providing the contents to be rendered.
     * @param contentView A {@link ContentView} that can handle user inputs.
     * @param attachParams A {@link ThinWebViewAttachParams} for additional parameters.
     */
    void attachWebContents(
            WebContents webContents, View contentView, ThinWebViewAttachParams attachParams);

    /**
     * Sets opacity for the view. {@link ThinWebViewConstraints#supportsOpacity} must be true for
     * using this method.
     */
    void setAlpha(float alpha);

    /**
     * Registers a callback that is run when the next frame successfully makes it to the screen.
     *
     * <p>This may be useful for operations that should be synchronized to renders that occur after
     * a layout change.
     *
     * @param runnable The runnable to be run.
     */
    void runOnNextFrame(Runnable runnable);

    /** Should be called for cleanup when the CompositorView instance is no longer used. */
    void destroy();
}
