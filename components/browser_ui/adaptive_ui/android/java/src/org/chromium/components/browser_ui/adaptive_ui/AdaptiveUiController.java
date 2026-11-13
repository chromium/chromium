// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.adaptive_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Presents {@link AdaptiveUiEntry}s: picks the container for each entry and shows it there. There
 * is one controller per {@link WindowAndroid}, and it handles entries of every {@link
 * AdaptiveUiScope}; the scope only decides when an entry is closed automatically.
 */
@NullMarked
public interface AdaptiveUiController {
    /**
     * Returns the controller attached to {@code windowAndroid}, or null if none is attached.
     *
     * @param windowAndroid The window to get the controller from.
     */
    static @Nullable AdaptiveUiController from(WindowAndroid windowAndroid) {
        return AdaptiveUiControllerProvider.from(windowAndroid);
    }

    /**
     * Requests that {@code entry} be shown. The entry may wait in the chosen container's queue;
     * {@link AdaptiveUiObserver#onShown} fires once it is visible.
     *
     * @param entry The entry to show.
     * @return The container that is now showing the entry or actively taking over for it (such as
     *     superseding a lower-precedence sheet); null if the entry was queued behind existing
     *     content, suppressed, or dropped (e.g. already showing or queued, or destroyed). A queued
     *     entry learns its container from {@link AdaptiveUiObserver#onShown}.
     */
    @Nullable
    @AdaptiveUiContainer
    Integer show(AdaptiveUiEntry entry);

    /**
     * Dismisses {@code entry} if it is showing or queued. {@link AdaptiveUiObserver#onDismissed}
     * fires with {@code reason}.
     *
     * @param entry The entry to dismiss.
     * @param reason Why the feature dismissed the entry.
     */
    void dismiss(AdaptiveUiEntry entry, @DismissReason int reason);
}
