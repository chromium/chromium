// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for LazySubscriptionsManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class LazySubscriptionsManagerTest {
    /** Tests the persistence of the "hasPersistedMessages" flag. */
    @Test
    public void testHasPersistedMessages() {
        final String subscriptionId = "subscription_id";
        // By default there is no persisted messages.
        assertTrue(
                LazySubscriptionsManager.getSubscriptionIdsWithPersistedMessages(subscriptionId)
                        .isEmpty());

        LazySubscriptionsManager.storeHasPersistedMessagesForSubscription(subscriptionId, true);
        assertEquals(
                1,
                LazySubscriptionsManager.getSubscriptionIdsWithPersistedMessages(subscriptionId)
                        .size());

        LazySubscriptionsManager.storeHasPersistedMessagesForSubscription(subscriptionId, false);
        assertTrue(
                LazySubscriptionsManager.getSubscriptionIdsWithPersistedMessages(subscriptionId)
                        .isEmpty());
    }

    /** Tests that lazy subscriptions are stored. */
    @Test
    public void testMarkSubscriptionAsLazy() {
        final String subscriptionId = "subscription_id";
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, true);
        assertTrue(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
    }

    /** Tests that unlazy subscriptions are stored. */
    @Test
    public void testMarkSubscriptionAsNotLazy() {
        final String subscriptionId = "subscription_id";
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, false);
        assertFalse(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
    }

    /** Tests subscriptions are not lazy be default. */
    @Test
    public void testDefaultSubscriptionNotLazy() {
        final String subscriptionId = "subscription_id";
        assertFalse(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
    }

    /** Tests that switching from lazy to unlazy should leave no queued messages behind. */
    @Test
    public void testSwitchingFromLazyToUnlazy() {
        final String subscriptionId = "subscription_id";
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, true);

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", "CollapseKey");
        GCMMessage message = new GCMMessage("MySenderId", extras);

        LazySubscriptionsManager.persistMessage(subscriptionId, message);
        assertEquals(1, LazySubscriptionsManager.readMessages(subscriptionId).length);

        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, false);
        assertEquals(0, LazySubscriptionsManager.readMessages(subscriptionId).length);
    }

    /** Tests that switching from lazy to unlazy and back to lazy. */
    @Test
    public void testSwitchingFromLazyToUnlazyAndBackToLazy() {
        final String subscriptionId = "subscription_id";
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, true);
        assertTrue(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, false);
        assertFalse(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
        LazySubscriptionsManager.storeLazinessInformation(subscriptionId, true);
        assertTrue(LazySubscriptionsManager.isSubscriptionLazy(subscriptionId));
    }

    /** Tests that GCM messages are persisted and read. */
    @Test
    public void testReadingPersistedMessage() {
        final String subscriptionId = "subscriptionId";
        final String anotherSubscriptionId = "AnotherSubscriptionId";

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", "CollapseKey");
        GCMMessage message = new GCMMessage("MySenderId", extras);
        LazySubscriptionsManager.persistMessage(subscriptionId, message);

        GCMMessage[] messages = LazySubscriptionsManager.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertEquals(message.getSenderId(), messages[0].getSenderId());

        messages = LazySubscriptionsManager.readMessages(anotherSubscriptionId);
        assertEquals(0, messages.length);
    }

    /** Tests that only MESSAGES_QUEUE_SIZE messages are kept. */
    @Test
    public void testPersistingMessageCount() {
        // This tests persists MESSAGES_QUEUE_SIZE+extraMessagesCount messages
        // and checks if only the most recent |MESSAGES_QUEUE_SIZE| are read.
        // |collapse_key| is used to distinguish between messages for
        // simplicity.
        final String subscriptionId = "subscriptionId";
        final String collapseKeyPrefix = "subscriptionId";
        final int extraMessagesCount = 5;

        // Persist |MESSAGES_QUEUE_SIZE| + |extraMessagesCount| messages.
        for (int i = 0;
                i < LazySubscriptionsManager.MESSAGES_QUEUE_SIZE + extraMessagesCount;
                i++) {
            Bundle extras = new Bundle();
            extras.putString("subtype", "MyAppId");
            extras.putString("collapse_key", collapseKeyPrefix + i);
            GCMMessage message = new GCMMessage("MySenderId", extras);
            LazySubscriptionsManager.persistMessage(subscriptionId, message);
        }
        // Check that only the most recent |MESSAGES_QUEUE_SIZE| are persisted.
        GCMMessage[] messages = LazySubscriptionsManager.readMessages(subscriptionId);
        assertEquals(LazySubscriptionsManager.MESSAGES_QUEUE_SIZE, messages.length);
        for (int i = 0; i < LazySubscriptionsManager.MESSAGES_QUEUE_SIZE; i++) {
            assertEquals(
                    collapseKeyPrefix + (i + extraMessagesCount), messages[i].getCollapseKey());
        }
    }

    /** Tests that messages with the same collapse key override each other. */
    @Test
    public void testCollapseKeyCollision() {
        final String subscriptionId = "subscriptionId";
        final String collapseKey = "collapseKey";
        final byte[] rawData1 = {0x00, 0x15, 0x30, 0x01};
        final byte[] rawData2 = {0x00, 0x15, 0x30, 0x02};

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", collapseKey);
        extras.putByteArray("rawData", rawData1);

        // Persist a message and make sure it's persisted.
        GCMMessage message1 = new GCMMessage("MySenderId", extras);
        LazySubscriptionsManager.persistMessage(subscriptionId, message1);

        GCMMessage[] messages = LazySubscriptionsManager.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertArrayEquals(rawData1, messages[0].getRawData());

        // Persist another message with the same collapse key and another raw data.
        extras.putByteArray("rawData", rawData2);
        GCMMessage message2 = new GCMMessage("MySenderId", extras);
        LazySubscriptionsManager.persistMessage(subscriptionId, message2);

        messages = LazySubscriptionsManager.readMessages(subscriptionId);
        assertEquals(1, messages.length);
        assertArrayEquals(rawData2, messages[0].getRawData());
    }

    /** Tests that messages with the same collapse key override each other. */
    @Test
    public void testDeletePersistedMessages() {
        final String subscriptionId = "subscriptionId";

        Bundle extras = new Bundle();
        extras.putString("subtype", "MyAppId");
        extras.putString("collapse_key", "collapseKey");
        extras.putByteArray("rawData", new byte[] {});
        GCMMessage message = new GCMMessage("MySenderId", extras);
        LazySubscriptionsManager.persistMessage(subscriptionId, message);

        assertEquals(1, LazySubscriptionsManager.readMessages(subscriptionId).length);
        LazySubscriptionsManager.deletePersistedMessagesForSubscriptionId(subscriptionId);
        assertEquals(0, LazySubscriptionsManager.readMessages(subscriptionId).length);
    }
}
