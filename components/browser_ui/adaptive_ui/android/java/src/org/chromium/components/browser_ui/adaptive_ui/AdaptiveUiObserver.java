// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;

/** Lifecycle callbacks for an {@link AdaptiveUiEntry}, shared by every container. */
@NullMarked
public interface AdaptiveUiObserver {
    /**
     * Called at most once per {@link AdaptiveUiController#show}, after the container is chosen and
     * before the entry's content is attached. Not called if the entry is closed first.
     *
     * @param container The container that will present the entry.
     */
    default void onWillShow(@AdaptiveUiContainer int container) {}

    /**
     * Called at most once per {@link AdaptiveUiController#show}, when the entry becomes visible.
     * Not called if the entry is closed while it still waits in a queue.
     */
    default void onShown() {}

    /**
     * Called exactly once per {@link AdaptiveUiController#show}, when the entry is closed, whether
     * or not it was ever shown. Features can rely on it to release what they set up for the entry.
     *
     * @param reason Why the entry was closed.
     */
    void onDismissed(@DismissReason int reason);
}
