// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Contains an object of either local ID or sync GUID, never both. */
@NullMarked
public class EitherId {

    /** The ID type for tab IDs. */
    public static class EitherTabId extends EitherId {
        // Must match kInvalidTabId definition in
        // components/collaboration/internal/messaging/messaging_backend_service_bridge.cc.
        public static final int INVALID_TAB_ID = -1;

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

        public static EitherTabId createSyncId(String syncId) {
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
        private final @Nullable LocalTabGroupId mLocalId;

        // Must provide either localId or syncId.
        private EitherGroupId(@Nullable LocalTabGroupId localId, @Nullable String syncId) {
            super(syncId);
            mLocalId = localId;
        }

        public static EitherGroupId createLocalId(LocalTabGroupId localId) {
            assert localId != null;
            return new EitherGroupId(localId, null);
        }

        public static EitherGroupId createSyncId(String syncId) {
            assert syncId != null;
            return new EitherGroupId(null, syncId);
        }

        @EnsuresNonNullIf("mLocalId")
        public boolean isLocalId() {
            return mLocalId != null;
        }

        public LocalTabGroupId getLocalId() {
            assert isLocalId();
            return mLocalId;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof EitherGroupId)) return false;
            EitherGroupId eitherId = (EitherGroupId) o;

            boolean localIdEqual =
                    isLocalId()
                            && eitherId.isLocalId()
                            && getLocalId().equals(eitherId.getLocalId());
            boolean syncIdEqual =
                    isSyncId() && eitherId.isSyncId() && getSyncId().equals(eitherId.getSyncId());
            return localIdEqual || syncIdEqual;
        }
    }

    private final @Nullable String mSyncId;

    private EitherId(@Nullable String syncId) {
        mSyncId = syncId;
    }

    @EnsuresNonNullIf("mSyncId")
    public boolean isSyncId() {
        return mSyncId != null;
    }

    public String getSyncId() {
        assert isSyncId();
        return mSyncId;
    }
}
