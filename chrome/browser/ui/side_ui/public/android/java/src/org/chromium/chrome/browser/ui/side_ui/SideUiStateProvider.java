// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Provider for SideUi state. Primarily, the {@link SideUiSpecs} through observers and a getter. */
@NullMarked
public interface SideUiStateProvider {

    /**
     * Adds a {@link SideUiObserver}. The provided observer will be notified whenever a new {@link
     * SideUiSpecs} is determined as a result of a change in a registered {@link SideUiContainer}.
     *
     * <p>Upon being added, the provided observer will also be notified of the current state of the
     * side UI so that it may immediately position itself accordingly. e.g. This accounts for the
     * case where an observer registers itself while some {@link SideUiContainer} is already showing
     * and there is no incoming update request that would trigger a notification for observers.
     *
     * <p>This is no-op (including being notified of the current {@link SideUiSpecs}) if the
     * provided observer was already registered.
     *
     * @param observer The {@link SideUiObserver} to add.
     */
    void addObserver(SideUiObserver observer);

    /**
     * Removes a {@link SideUiObserver}.
     *
     * <p>Upon removal, the provided observer will be notified as if there were no side UI present.
     * i.e. it will be passed a {@link SideUiSpecs#EMPTY_SIDE_UI_SPECS}, which represents the specs
     * when no side UI is currently showing. The intent of this is to attempt to return a given
     * observer to its state prior to observing Side UI.
     *
     * <p>This is no-op (including not being notified of empty Side UI specs) if the provided
     * observer was not actually present in the list of observers.
     *
     * @param observer The {@link SideUiObserver} to remove.
     */
    void removeObserver(SideUiObserver observer);

    /**
     * Returns the current {@link SideUiSpecs}. Most clients should instead register themselves as a
     * {@link SideUiObserver}. This is primarily intended to be used by the CompositorViewHolder,
     * which requires the current specs separate from when it's passed through the observer event.
     *
     * @return The current {@link SideUiSpecs}.
     */
    SideUiSpecs getCurrentSideUiSpecs();
}
