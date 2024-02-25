// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.thinwebview;

import android.graphics.Color;

/** Various constraints associated with the thin webview based on the usage. */
public class ThinWebViewConstraints implements Cloneable {
    /** Whether this view will support opacity. */
    public boolean supportsOpacity;

    /** Background color of this view. */
    public int backgroundColor = Color.WHITE;

    @Override
    public ThinWebViewConstraints clone() {
        ThinWebViewConstraints clone = new ThinWebViewConstraints();
        clone.supportsOpacity = supportsOpacity;
        clone.backgroundColor = backgroundColor;
        return clone;
    }
}
