// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.tab_group_sync.proto.TabGroupIdMetadata.TabGroupIDMetadataProto;

import java.util.Map;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupMetadataPersistentStoreUnitTest {

    @Test
    public void testWriteAndReadMatches() {
        TabGroupMetadataPersistentStore store = new TabGroupMetadataPersistentStore();

        TabGroupIDMetadataProto source =
                TabGroupIDMetadataProto.newBuilder()
                        .setSerializedToken("mySerializedToken")
                        .build();
        store.write("myGuid", source);

        Map<String, TabGroupIDMetadataProto> entries = store.readAll();
        assertEquals(1, entries.keySet().size());
        assertTrue(entries.keySet().contains("myGuid"));

        TabGroupIDMetadataProto dest = entries.get("myGuid");
        assertEquals("mySerializedToken", dest.getSerializedToken());
    }

    @Test
    public void testWriteAndDelete() {
        TabGroupMetadataPersistentStore store = new TabGroupMetadataPersistentStore();

        TabGroupIDMetadataProto source =
                TabGroupIDMetadataProto.newBuilder()
                        .setSerializedToken("mySerializedToken")
                        .build();
        store.write("myGuid", source);
        store.delete("myGuid");

        Map<String, TabGroupIDMetadataProto> entries = store.readAll();
        assertEquals(0, entries.keySet().size());
    }

    @Test
    public void testMultipleWritesAndReads() {
        TabGroupMetadataPersistentStore store = new TabGroupMetadataPersistentStore();

        TabGroupIDMetadataProto source1 =
                TabGroupIDMetadataProto.newBuilder()
                        .setSerializedToken("mySerializedToken1")
                        .build();
        store.write("myGuid1", source1);

        TabGroupIDMetadataProto source2 =
                TabGroupIDMetadataProto.newBuilder()
                        .setSerializedToken("mySerializedToken2")
                        .build();
        store.write("myGuid2", source2);

        Map<String, TabGroupIDMetadataProto> entries = store.readAll();
        assertEquals(2, entries.keySet().size());
        assertTrue(entries.keySet().contains("myGuid1"));
        assertTrue(entries.keySet().contains("myGuid2"));

        TabGroupIDMetadataProto dest1 = entries.get("myGuid1");
        assertEquals("mySerializedToken1", dest1.getSerializedToken());
        TabGroupIDMetadataProto dest2 = entries.get("myGuid2");
        assertEquals("mySerializedToken2", dest2.getSerializedToken());
    }
}
