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
 * (components/saved_tab_groups/saved_tab_group.h) class.
 */
public class SavedTabGroup {
    /** The ID used to represent the tab group in sync. */
    // TODO(shaktisahu): Decide if this will be used from Java to native flow. If yes, this ID
    //  can be nullable as well.
    public String syncId;

    /**
     * The ID representing the tab group locally in the tab model. Currently, it's the root ID as
     * returned by {@link Tab#getRootId()}. In near future, it will be replaced by {@link
     * Tab#getTabGroupId()} instead. This field can be null if the {@link SavedTabGroup} represents
     * a tab group that isn't present local tab model yet.
     */
    public @Nullable Integer localId;

    /** The title of the tab group. */
    public @Nullable String title;

    /** The color of the tab group. */
    public @TabGroupColorId int color;

    /** Timestamp for when the tab was created. */
    public long creationTimeMs;

    /** Timestamp for when the tab was last updated. */
    public long updateTimeMs;

    /** The tabs associated with this saved tab group. */
    public List<SavedTabGroupTab> savedTabs = new ArrayList<>();
}
