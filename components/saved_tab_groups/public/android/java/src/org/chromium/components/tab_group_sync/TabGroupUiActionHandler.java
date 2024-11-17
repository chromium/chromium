// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

/**
 * Interface to handle tab group UI actions such as open tab groups from tab group revisit surface.
 */
public interface TabGroupUiActionHandler {

    /**
     * Opens a tab group locally that matches the remote tab group associated with {@code syncId}.
     *
     * @param syncId The sync ID associated with the tab group to be opened.
     */
    void openTabGroup(String syncId);
}
