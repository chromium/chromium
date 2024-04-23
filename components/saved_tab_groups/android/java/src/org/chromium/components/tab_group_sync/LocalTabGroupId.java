// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * LocalTabGroupId is a convenient class to contain all the information needed to uniquely identify
 * a the local tab group.
 */
public class LocalTabGroupId {
    // The root ID of the tab group. Soon to be deprecated.
    public @NonNull final Integer rootId;

    /**
     * Constructor.
     *
     * @param rootId The root ID of the tab group in {@link TabModel}.
     */
    public LocalTabGroupId(int rootId) {
        this.rootId = rootId;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof LocalTabGroupId)) return false;

        LocalTabGroupId other = (LocalTabGroupId) o;
        return rootId.equals(other.rootId);
    }

    @Override
    public int hashCode() {
        return Integer.valueOf(rootId).hashCode();
    }
}
