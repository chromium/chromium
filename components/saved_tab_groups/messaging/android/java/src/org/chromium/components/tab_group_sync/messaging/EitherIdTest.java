// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.messaging.EitherId.EitherTabId;

/** Test for both sub classes of EitherId. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(value = PER_CLASS)
public class EitherIdTest {
    @Test
    public void testCreatingLocalTabId() {
        EitherTabId tabId = EitherTabId.createLocalId(30);
        Assert.assertTrue(tabId.isLocalId());
        Assert.assertFalse(tabId.isSyncId());
        Assert.assertEquals(30, tabId.getLocalId());
    }

    @Test
    public void testCreatingSyncTabId() {
        EitherTabId tabId = EitherTabId.createSyncId("123");
        Assert.assertFalse(tabId.isLocalId());
        Assert.assertTrue(tabId.isSyncId());
        Assert.assertEquals("123", tabId.getSyncId());
    }

    @Test
    public void testCreatingNullTabIdsFail() {
        Assert.assertThrows(
                AssertionError.class, () -> EitherTabId.createLocalId(EitherTabId.INVALID_TAB_ID));
        Assert.assertThrows(AssertionError.class, () -> EitherTabId.createSyncId(null));
    }

    @Test
    public void testCreatingLocalGroupId() {
        LocalTabGroupId localTabGroupId = new LocalTabGroupId(Token.createRandom());
        EitherGroupId groupId = EitherGroupId.createLocalId(localTabGroupId);
        Assert.assertTrue(groupId.isLocalId());
        Assert.assertFalse(groupId.isSyncId());
        Assert.assertEquals(localTabGroupId, groupId.getLocalId());
    }

    @Test
    public void testCreatingSyncGroupId() {
        EitherGroupId groupId = EitherGroupId.createSyncId("123");
        Assert.assertFalse(groupId.isLocalId());
        Assert.assertTrue(groupId.isSyncId());
        Assert.assertEquals("123", groupId.getSyncId());
    }

    @Test
    public void testCreatingNullGroupIdsFail() {
        Assert.assertThrows(AssertionError.class, () -> EitherGroupId.createLocalId(null));
        Assert.assertThrows(AssertionError.class, () -> EitherGroupId.createSyncId(null));
    }
}
