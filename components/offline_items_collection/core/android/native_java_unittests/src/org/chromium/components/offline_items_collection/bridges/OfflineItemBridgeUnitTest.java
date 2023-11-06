// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection.bridges;

import org.jni_zero.CalledByNative;
import org.junit.Assert;

import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.PendingState;

/**
 * Unit test to verify {@link OfflineItemBridge} can create {@link OfflineItem} correctly through
 * JNI bridge from native. See native unit test:
 * (components/offline_items_collection/core/android/offline_item_bridge_unittest.cc).
 */
public class OfflineItemBridgeUnitTest {
    @CalledByNative
    private OfflineItemBridgeUnitTest() {}

    @CalledByNative
    public void testCreateDefaultOfflineItem(OfflineItem item) {
        // Verifies key fields for a default offline item.
        Assert.assertNotNull(item);
        Assert.assertEquals(OfflineItemFilter.OTHER, item.filter);
        Assert.assertFalse(item.isTransient);
        Assert.assertEquals(OfflineItemState.COMPLETE, item.state);
        Assert.assertEquals(FailState.NO_FAILURE, item.failState);
        Assert.assertEquals(PendingState.NOT_PENDING, item.pendingState);
    }
}
