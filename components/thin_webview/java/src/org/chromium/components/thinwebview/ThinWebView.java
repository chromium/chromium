// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;

/**
 * An android view backed by a {@link Surface} that is able to display a cc::Layer. Either, a {@link
 * TextureView} or {@link SurfaceView} can be used to provide the surface. The cc::Layer should be
 * provided in the native.
 */
@NullMarked
public interface ThinWebView {
    /**
     * @return The android {@link View} representing this widget.
     */
    View getView();

    /**
     * Method to be called to display the contents of a {@link WebContents} on the surface. The user
     * interactability is provided through the {@code contentView}.
     *
     * @param webContents A {@link WebContents} for providing the contents to be rendered.
     * @param contentView A {@link ContentView} that can handle user inputs.
     * @param delegate A {@link WebContentsDelegateAndroid} responding to requests on WebContents.
     *     This is recommended for the WebContents created explicitly for ThisWebView, in case it
     *     needs one.
     */
    void attachWebContents(
            WebContents webContents,
            @Nullable View contentView,
            @Nullable WebContentsDelegateAndroid delegate);

    /**
     * Method to be called to display the contents of a {@link WebContents} on the surface. The user
     * interactability is provided through the {@code contentView}.
     *
     * @param webContents A {@link WebContents} for providing the contents to be rendered.
     * @param contentView A {@link ContentView} that can handle user inputs.
     * @param delegate A {@link WebContentsDelegateAndroid} responding to requests on WebContents.
     *     This is recommended for the WebContents created explicitly for ThisWebView, in case it
     *     needs one.
     * @param contextMenuPopulatorFactory A {@link ContextMenuPopulatorFactory} required to display
     *     the long press context menu.
     */
    void attachWebContents(
            WebContents webContents,
            @Nullable View contentView,
            @Nullable WebContentsDelegateAndroid delegate,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate);

    /**
     * Sets opacity for the view. {@link ThinWebViewConstraints#supportsOpacity} must be true for
     * using this method.
     */
    void setAlpha(float alpha);

    /**
     * Sets the insets for the WebContents. This reduces the viewport size of the WebContents and
     * offsets it. The physical backing (surface) remains the size of the View.
     *
     * @param top The top inset.
     * @param left The left inset.
     * @param bottom The bottom inset.
     * @param right The right inset.
     */
    void setInsets(int top, int left, int bottom, int right);

    /** Should be called for cleanup when the CompositorView instance is no longer used. */
    void destroy();
}
