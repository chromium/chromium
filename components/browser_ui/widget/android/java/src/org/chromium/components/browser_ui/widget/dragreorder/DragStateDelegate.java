// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

/**
 * Responsible for keeping track of the drag state (whether drag is enabled, and if so, whether drag
 * is active).
 */
public interface DragStateDelegate {
    /**
     * Gets whether drag is enabled. If true, the UI may enter a state where items can be dragged,
     * but items are not necessarily currently draggable. Items will only become draggable after
     * drag is activated (see {@link #getDragActive}).
     */
    boolean getDragEnabled();

    /**
     * Gets whether drag is currently active (e.g. the UI is in a state where items can be
     * dragged). Activating drag is only valid if drag is currently enabled.
     */
    boolean getDragActive();
}
