// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;

/** Lifecycle callbacks for an {@link AdaptiveUiEntry}, shared by every container. */
@NullMarked
public interface AdaptiveUiObserver {
    /**
     * Called at most once per {@link AdaptiveUiController#show}, when the container starts showing
     * the entry (a dialog right after it is shown, a sheet once it takes the entry's content). Not
     * called if the entry is closed while it still waits in a queue, and not called again if the
     * container temporarily hides the entry and shows it again.
     *
     * @param container The container that presents the entry.
     */
    default void onShown(@AdaptiveUiContainer int container) {}

    /**
     * Called exactly once per {@link AdaptiveUiController#show}, when the entry is closed, whether
     * or not it was ever shown. Features can rely on it to release what they set up for the entry.
     *
     * @param reason Why the entry was closed.
     */
    void onDismissed(@DismissReason int reason);
}
