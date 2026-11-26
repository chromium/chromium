// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.build.annotations.NullMarked;

/**
 * Interface to handle tab group UI actions such as open tab groups from tab group revisit surface.
 */
@NullMarked
public interface TabGroupUiActionHandler {

    /**
     * Opens a tab group locally that matches the remote tab group associated with {@code syncId},
     * recording {@code openingSource}.
     *
     * @param syncId The sync ID of the tab group to open.
     * @param openingSource The UI surface requesting the open.
     * @return True if the group opened, false if it is already open or could not open.
     */
    boolean openTabGroup(String syncId, @OpeningSource int openingSource);
}
