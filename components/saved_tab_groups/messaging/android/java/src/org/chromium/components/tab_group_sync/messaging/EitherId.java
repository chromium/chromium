// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.components.tab_group_sync.LocalTabGroupId;

/** Contains an object of either local ID or sync GUID, never both. */
public class EitherId {

    /** The ID type for tab IDs. */
    public static class EitherTabId extends EitherId {
        // Must match kInvalidTabId definition in
        // components/saved_tab_groups/messaging/messaging_backend_service_bridge.cc.
        /* package */ static final int INVALID_TAB_ID = -1;

        private final int mLocalId;

        // Must provide either localId or syncId.
        private EitherTabId(int localId, @Nullable String syncId) {
            super(syncId);
            mLocalId = localId;
        }

        /** The localId can not be -1. */
        public static EitherTabId createLocalId(int localId) {
            assert localId != INVALID_TAB_ID;
            return new EitherTabId(localId, null);
        }

        public static EitherTabId createSyncId(@NonNull String syncId) {
            assert syncId != null;
            return new EitherTabId(INVALID_TAB_ID, syncId);
        }

        public boolean isLocalId() {
            return mLocalId != INVALID_TAB_ID;
        }

        public int getLocalId() {
            assert isLocalId();
            return mLocalId;
        }
    }

    /** The ID type for tab group IDs. */
    public static class EitherGroupId extends EitherId {
        private final LocalTabGroupId mLocalId;

        // Must provide either localId or syncId.
        private EitherGroupId(@Nullable LocalTabGroupId localId, @Nullable String syncId) {
            super(syncId);
            mLocalId = localId;
        }

        public static EitherGroupId createLocalId(@NonNull LocalTabGroupId localId) {
            assert localId != null;
            return new EitherGroupId(localId, null);
        }

        public static EitherGroupId createSyncId(@NonNull String syncId) {
            assert syncId != null;
            return new EitherGroupId(null, syncId);
        }

        public boolean isLocalId() {
            return mLocalId != null;
        }

        @NonNull
        public LocalTabGroupId getLocalId() {
            assert isLocalId();
            return mLocalId;
        }
    }

    @Nullable private final String mSyncId;

    private EitherId(@Nullable String syncId) {
        mSyncId = syncId;
    }

    public boolean isSyncId() {
        return mSyncId != null;
    }

    @NonNull
    public String getSyncId() {
        assert isSyncId();
        return mSyncId;
    }
}
