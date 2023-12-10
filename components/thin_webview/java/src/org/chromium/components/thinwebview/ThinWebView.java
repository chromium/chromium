// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
public interface ThinWebView {
    /**@return The android {@link View} representing this widget. */
    View getView();

    /**
     * Method to be called to display the contents of a {@link WebContents} on the surface. The user
     * interactability is provided through the {@code contentView}.
     * @param webContents A {@link WebContents} for providing the contents to be rendered.
     * @param contentView A {@link ContentView} that can handle user inputs.
     * @param delegate A {@link WebContentsDelegateAndroid} responding to requests on WebContents.
     *        This is recommended for the WebContents created explicitly for ThisWebView, in case
     *        it needs one.
     */
    void attachWebContents(
            WebContents webContents,
            @Nullable View contentView,
            @Nullable WebContentsDelegateAndroid delegate);

    /**
     * Sets opacity for the view. {@link ThinWebViewConstraints#supportsOpacity} must be true for
     * using this method.
     */
    void setAlpha(float alpha);

    /** Should be called for cleanup when the CompositorView instance is no longer used. */
    void destroy();
}
