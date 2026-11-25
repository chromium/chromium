// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_groups.TabGroupId;

import java.util.Objects;

/**
 * LocalTabGroupId is a convenient class to contain all the information needed to uniquely identify
 * a the local tab group.
 */
@NullMarked
public class LocalTabGroupId {
    // Stable ID of the tab group. This should be used going forward.
    public final TabGroupId tabGroupId;

    /**
     * Constructor.
     *
     * @param tabGroupId The stable ID of the tab group in {@link TabModel}.
     */
    public LocalTabGroupId(TabGroupId tabGroupId) {
        assert tabGroupId != null;
        this.tabGroupId = tabGroupId;
    }

    /**
     * @param tabGroupId The raw {@link Token} of the tab group.
     * @deprecated Temporary overload for downstream callers until {@code Tab#getTabGroupId()}
     *     returns {@link TabGroupId}.
     */
    // TODO(crbug.com/585285216): Remove once Tab#getTabGroupId() returns TabGroupId.
    @Deprecated
    public LocalTabGroupId(Token tabGroupId) {
        this(TabGroupId.fromRawToken(tabGroupId));
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof LocalTabGroupId)) return false;

        LocalTabGroupId other = (LocalTabGroupId) o;
        return Objects.equals(tabGroupId, other.tabGroupId);
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(tabGroupId);
    }

    @Override
    public String toString() {
        return tabGroupId.toString();
    }
}
