// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.messaging;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link MessageUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MessageUtilsUnitTest {
    private static final String MESSAGE_ID = "Message ID";
    private static final int TAB_ID = 1;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final String GIVEN_NAME = "Jane";
    private static final String TAB_TITLE = "Tab Title";
    private static final String TAB_GROUP_TITLE = "Group Title";
    private static final String TAB_GROUP_SYNC_ID = "Group Sync ID";

    @Test
    public void testExtractTabId() {
        PersistentMessage message = new PersistentMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabMetadata = new TabMessageMetadata();
        message.attribution.tabMetadata.localTabId = TAB_ID;
        assertEquals(TAB_ID, MessageUtils.extractTabId(message));

        message.attribution.tabMetadata = null;
        assertEquals(TabMessageMetadata.INVALID_TAB_ID, MessageUtils.extractTabId(message));

        message.attribution = null;
        assertEquals(TabMessageMetadata.INVALID_TAB_ID, MessageUtils.extractTabId(message));

        assertEquals(TabMessageMetadata.INVALID_TAB_ID, MessageUtils.extractTabId(null));
    }

    @Test
    public void testExtractTabGroupId_Persistent() {
        PersistentMessage message = new PersistentMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        assertEquals(TAB_GROUP_ID, MessageUtils.extractTabGroupId(message));

        message.attribution.tabGroupMetadata = null;
        assertEquals(null, MessageUtils.extractTabGroupId(message));

        message.attribution = null;
        assertEquals(null, MessageUtils.extractTabGroupId(message));

        assertEquals(null, MessageUtils.extractTabGroupId((PersistentMessage) null));
    }

    @Test
    public void testExtractTabGroupId_Instant() {
        InstantMessage message = new InstantMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        assertEquals(TAB_GROUP_ID, MessageUtils.extractTabGroupId(message));

        message.attribution.tabGroupMetadata = null;
        assertEquals(null, MessageUtils.extractTabGroupId(message));

        message.attribution = null;
        assertEquals(null, MessageUtils.extractTabGroupId(message));

        assertEquals(null, MessageUtils.extractTabGroupId((InstantMessage) null));
    }

    @Test
    public void testExtractGivenName() {
        InstantMessage message = new InstantMessage();
        message.attribution = new MessageAttribution();
        message.attribution.triggeringUser =
                new GroupMember(null, null, null, MemberRole.OWNER, null, GIVEN_NAME);
        assertEquals(GIVEN_NAME, MessageUtils.extractGivenName(message));

        message.attribution.triggeringUser = null;
        assertEquals("", MessageUtils.extractGivenName(message));

        message.attribution = null;
        assertEquals("", MessageUtils.extractGivenName(message));

        assertEquals("", MessageUtils.extractGivenName(null));
    }

    @Test
    public void testExtractTabTitle() {
        InstantMessage message = new InstantMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabMetadata = new TabMessageMetadata();
        message.attribution.tabMetadata.lastKnownTitle = TAB_TITLE;
        assertEquals(TAB_TITLE, MessageUtils.extractTabTitle(message));

        message.attribution.tabMetadata = null;
        assertEquals("", MessageUtils.extractTabTitle(message));

        message.attribution = null;
        assertEquals("", MessageUtils.extractTabTitle(message));

        assertEquals("", MessageUtils.extractTabTitle(null));
    }

    @Test
    public void testExtractTabGroupTitle_Instant() {
        InstantMessage message = new InstantMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.lastKnownTitle = TAB_GROUP_TITLE;
        assertEquals(TAB_GROUP_TITLE, MessageUtils.extractTabGroupTitle(message));

        message.attribution.tabGroupMetadata = null;
        assertEquals("", MessageUtils.extractTabGroupTitle(message));

        message.attribution = null;
        assertEquals("", MessageUtils.extractTabGroupTitle(message));

        assertEquals("", MessageUtils.extractTabGroupTitle((InstantMessage) null));
    }

    @Test
    public void testExtractTabGroupTitle_Persistent() {
        PersistentMessage message = new PersistentMessage();
        message.attribution = new MessageAttribution();
        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        message.attribution.tabGroupMetadata.lastKnownTitle = TAB_GROUP_TITLE;
        assertEquals(TAB_GROUP_TITLE, MessageUtils.extractTabGroupTitle(message));

        message.attribution.tabGroupMetadata = null;
        assertEquals("", MessageUtils.extractTabGroupTitle(message));

        message.attribution = null;
        assertEquals("", MessageUtils.extractTabGroupTitle(message));

        assertEquals("", MessageUtils.extractTabGroupTitle((PersistentMessage) null));
    }

    @Test
    public void testExtractMember() {
        assertEquals(null, MessageUtils.extractMember((InstantMessage) null));
        assertEquals(null, MessageUtils.extractMember((PersistentMessage) null));

        InstantMessage message = new InstantMessage();
        assertEquals(null, MessageUtils.extractMember(message));

        message.attribution = new MessageAttribution();
        assertEquals(null, MessageUtils.extractMember(message));

        message.attribution.triggeringUser = GROUP_MEMBER1;
        assertEquals(GROUP_MEMBER1, MessageUtils.extractMember(message));

        message.attribution.affectedUser = GROUP_MEMBER2;
        assertEquals(GROUP_MEMBER2, MessageUtils.extractMember(message));
    }

    @Test
    public void testExtractTabUrl() {
        assertEquals(null, MessageUtils.extractTabUrl(null));

        InstantMessage message = new InstantMessage();
        assertEquals(null, MessageUtils.extractTabUrl(message));

        message.attribution = new MessageAttribution();
        assertEquals(null, MessageUtils.extractTabUrl(message));

        message.attribution.tabMetadata = new TabMessageMetadata();
        assertEquals(null, MessageUtils.extractTabUrl(message));

        message.attribution.tabMetadata.lastKnownUrl = JUnitTestGURLs.URL_1.getSpec();
        assertEquals(JUnitTestGURLs.URL_1.getSpec(), MessageUtils.extractTabUrl(message));
    }

    @Test
    public void testExtractSyncTabGroupId_Instant() {
        assertNull(MessageUtils.extractSyncTabGroupId((InstantMessage) null));

        InstantMessage message = new InstantMessage();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution = new MessageAttribution();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution.tabGroupMetadata.syncTabGroupId = TAB_GROUP_SYNC_ID;
        assertEquals(TAB_GROUP_SYNC_ID, MessageUtils.extractSyncTabGroupId(message));
    }

    @Test
    public void testExtractSyncTabGroupId_Persistent() {
        assertNull(MessageUtils.extractSyncTabGroupId((PersistentMessage) null));

        PersistentMessage message = new PersistentMessage();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution = new MessageAttribution();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        assertNull(MessageUtils.extractSyncTabGroupId(message));

        message.attribution.tabGroupMetadata.syncTabGroupId = TAB_GROUP_SYNC_ID;
        assertEquals(TAB_GROUP_SYNC_ID, MessageUtils.extractSyncTabGroupId(message));
    }

    @Test
    public void testExtractMessageId_Instant() {
        InstantMessage message = new InstantMessage();
        message.attribution = new MessageAttribution();
        message.attribution.id = MESSAGE_ID;
        assertEquals(MESSAGE_ID, MessageUtils.extractMessageId(message));

        message.attribution.id = null;
        assertNull(MessageUtils.extractMessageId(message));

        message.attribution = null;
        assertNull(MessageUtils.extractMessageId(message));

        assertNull(MessageUtils.extractMessageId((InstantMessage) null));
    }

    @Test
    public void testExtractMessageId_Persistent() {
        PersistentMessage message = new PersistentMessage();
        message.attribution = new MessageAttribution();
        message.attribution.id = MESSAGE_ID;
        assertEquals(MESSAGE_ID, MessageUtils.extractMessageId(message));

        message.attribution.id = null;
        assertNull(MessageUtils.extractMessageId(message));

        message.attribution = null;
        assertNull(MessageUtils.extractMessageId(message));

        assertNull(MessageUtils.extractMessageId((PersistentMessage) null));
    }
}
