// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.graphics.Color;

import org.chromium.build.annotations.NullMarked;

/** Various constraints associated with the thin webview based on the usage. */
@NullMarked
public class ThinWebViewConstraints implements Cloneable {
    /** Whether this view will support opacity. */
    public boolean supportsOpacity;

    /** Background color of this view. */
    public int backgroundColor = Color.WHITE;

    /**
     * Whether to ignore size changes from layout changes. When set to true, web contents size
     * updates will only be performed by {@link ThinWebView#resizeWebContents}.
     *
     * <p>This may be desired when the native compositor view size needs to be different from the
     * actual size of the compositor view surface.
     */
    public boolean ignoreSizeChanges;

    @Override
    public ThinWebViewConstraints clone() {
        ThinWebViewConstraints clone = new ThinWebViewConstraints();
        clone.supportsOpacity = supportsOpacity;
        clone.backgroundColor = backgroundColor;
        clone.ignoreSizeChanges = ignoreSizeChanges;
        return clone;
    }
}
