// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import androidx.annotation.Nullable;

import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.List;

/**
 * This class is the Java counterpart to the C++ SavedTabGroup.
 * (components/saved_tab_groups/public/saved_tab_group.h) class.
 */
public class SavedTabGroup {
    /** The ID used to represent the tab group in sync. */
    // TODO(shaktisahu): Decide if this will be used from Java to native flow. If yes, this ID
    //  can be nullable as well.
    public String syncId;

    /**
     * The ID representing the tab group locally in the tab model. This field can be null if the
     * {@link SavedTabGroup} represents a tab group that isn't present local tab model yet.
     */
    public @Nullable LocalTabGroupId localId;

    /** The title of the tab group. */
    public @Nullable String title;

    /** The color of the tab group. */
    public @TabGroupColorId int color;

    /** Timestamp for when the tab was created. */
    public long creationTimeMs;

    /** Timestamp for when the tab was last updated. */
    public long updateTimeMs;

    /* The sync cache guid of the device that created the tab group. */
    public String creatorCacheGuid;

    /* The sync cache guid of the device that last updated the tab group. */
    public String lastUpdaterCacheGuid;

    /**
     * Collaboration group ID for Shared tab groups. This field can be null if the {@link
     * SavedTabGroup} represents a saved tab group which is not shared.
     */
    public @Nullable String collaborationId;

    /** The tabs associated with this saved tab group. */
    public List<SavedTabGroupTab> savedTabs = new ArrayList<>();

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Saved group: syncId = ");
        sb.append(syncId);
        sb.append(", localId = ");
        sb.append(localId);
        sb.append(", title = ");
        sb.append(title);
        sb.append(", color = ");
        sb.append(color);
        sb.append(", # of Tabs = ");
        sb.append(savedTabs.size());

        for (int i = 0; i < savedTabs.size(); i++) {
            sb.append("\nTab[");
            sb.append(i);
            sb.append("] -> ");
            sb.append(savedTabs.get(i));
        }
        return sb.toString();
    }
}
