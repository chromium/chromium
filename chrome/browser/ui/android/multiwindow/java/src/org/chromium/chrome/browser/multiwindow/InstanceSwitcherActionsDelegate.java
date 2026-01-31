// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Delegate for handling actions from the instance switcher UI. */
@NullMarked
public interface InstanceSwitcherActionsDelegate {
    /**
     * Opens the specified instance.
     *
     * @param instanceId The ID of the instance to open.
     */
    void openInstance(int instanceId);

    /**
     * Closes the specified instances.
     *
     * @param instanceIds The list of IDs of the instances to close.
     */
    void closeInstances(List<Integer> instanceIds);

    /**
     * Renames the specified instance.
     *
     * @param instanceId The ID of the instance to rename.
     * @param newName The new name for the instance.
     */
    void renameInstance(int instanceId, String newName);

    /**
     * Opens a new window.
     *
     * @param isIncognito Whether the new window should be incognito.
     */
    void openNewWindow(boolean isIncognito);
}
