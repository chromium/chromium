// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.paintpreview.player.frame;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;

/**
 * Sets up the view and the logic behind it for a Paint Preview frame.
 */
public class PlayerFrameCoordinator {
    /**
     * Creates a {@link PlayerFrameMediator} and {@link PlayerFrameView} for this component and
     * binds them together.
     */
    public PlayerFrameCoordinator(Context context, PlayerCompositorDelegate compositorDelegate,
            long frameGuid, int contentWidth, int contentHeight, boolean canDetectZoom) {}

    /**
     * Adds a child {@link PlayerFrameCoordinator} to this class.
     * @param subFrame The sub-frame's {@link PlayerFrameCoordinator}.
     * @param clipRect The {@link Rect} in which this sub-frame should be shown in.
     */
    public void addSubFrame(PlayerFrameCoordinator subFrame, Rect clipRect) {}

    public View getView() {
        return null;
    }
}
