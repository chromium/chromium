// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.scene_layer;

/** An extension of SceneLayer for SceneOverlay. */
public abstract class SceneOverlayLayer extends SceneLayer {
    /**
     * Sets a content tree inside this scene overlay tree.
     * TODO(jaekyun): We need to rename this method later because the meaning of "content" isn't
     * clear.
     */
    public abstract void setContentTree(SceneLayer contentTree);
}
