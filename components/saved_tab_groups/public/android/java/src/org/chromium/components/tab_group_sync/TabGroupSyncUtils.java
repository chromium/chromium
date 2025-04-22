// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Util methods for tab group sync service. */
@NullMarked
public class TabGroupSyncUtils {
    /**
     * Called to retrieve the {@link SavedTabGroup} associated with a collaboration ID.
     *
     * @param collaborationId The collaboration ID associated with the group.
     * @param tabGroupSyncService The sync service to get tab group data form.
     * @return The {@link SavedTabGroup} from sync service.
     */
    public @Nullable static SavedTabGroup getTabGroupForCollabIdFromSync(
            String collaborationId, TabGroupSyncService tabGroupSyncService) {
        for (String syncGroupId : tabGroupSyncService.getAllGroupIds()) {
            SavedTabGroup savedTabGroup = tabGroupSyncService.getGroup(syncGroupId);
            assumeNonNull(savedTabGroup);
            assert !savedTabGroup.savedTabs.isEmpty();
            if (savedTabGroup.collaborationId != null
                    && savedTabGroup.collaborationId.equals(collaborationId)) {
                return savedTabGroup;
            }
        }
        return null;
    }
}
