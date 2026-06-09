// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;

/** Provider for SideUi state. Primarily, the {@link SideUiSpecs} through observers and a getter. */
@NullMarked
public interface SideUiStateProvider {

    /**
     * Adds a {@link SideUiObserver}. The provided observer will be notified whenever a new {@link
     * SideUiSpecs} is determined as a result of a change in a registered {@link SideUiContainer}.
     *
     * <p>To receive all {@link SideUiSpecs} updates, observers should register themselves before
     * any SideUi is shown, typically during {@code ChromeActivity} initialization.
     *
     * <p>This is no-op if the provided observer was already registered.
     *
     * @param observer The {@link SideUiObserver} to add.
     */
    void addObserver(SideUiObserver observer);

    /**
     * Removes a {@link SideUiObserver}.
     *
     * <p>This is no-op if the provided observer was not actually present in the list of observers.
     *
     * @param observer The {@link SideUiObserver} to remove.
     */
    void removeObserver(SideUiObserver observer);

    /** Returns the current {@link SideUiSpecs}. */
    SideUiSpecs getCurrentSideUiSpecs();

    /** Returns whether the SideUIContainer of the given ID is currently showing. */
    boolean isSideUiShowing(@SideUiId int sideUidId);

    /**
     * Returns whether the SideUIContainer of the given ID can currently be shown given the window
     * width constraints. This checks ability to be shown, not whether it is currently showing (use
     * {@link #isSideUiShowing(int)} for the latter).
     */
    boolean canShowSideUi(@SideUiId int sideUiId);
}
