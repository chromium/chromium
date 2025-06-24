// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;
import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit test to verify JNI conversions required for {@link TabGroupSyncServiceImpl}. This includes
 * various tests for conversion of tab group and tab IDs, SavedTabGroup and SavedTabGroupTab, and
 * mutation and observation method calls for the service. See native unit test:
 * (components/saved_tab_groups/android/tab_group_sync_service_android_unittest.cc).
 */
@JNINamespace("tab_groups")
public class TabGroupSyncServiceAndroidUnitTest {
    private static final Token TOKEN_1 = new Token(4, 5);
    private static final String TEST_GROUP_TITLE = "Test Group";
    private static final String TEST_GROUP_TITLE_2 = "Test Group 2";
    private static final String TEST_TAB_TITLE = "Test Tab";
    private static final String TEST_URL = "https://google.com";
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final int LOCAL_TAB_ID_1 = 2;
    private static final int LOCAL_TAB_ID_2 = 4;
    private static final int TAB_POSITION = 3;
    private TabGroupSyncService mService;
    private TabGroupSyncService.Observer mObserver;
    private final ArgumentCaptor<SavedTabGroup> mTabGroupCaptor =
            ArgumentCaptor.forClass(SavedTabGroup.class);
    private Callback<Boolean> mCallback;

    @CalledByNative
    private TabGroupSyncServiceAndroidUnitTest() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private void setUpTestObserver(TabGroupSyncService service) {
        mService = service;
        Assert.assertNotNull(mService);
        mObserver = mock(TabGroupSyncService.Observer.class);
        mService.addObserver(mObserver);
    }

    @CalledByNative
    public void testOnInitialized() throws TimeoutException {
        verify(mObserver).onInitialized();
    }

    @CalledByNative
    public void testSavedTabGroupConversionNativeToJava(SavedTabGroup group) {
        Assert.assertNotNull(group);
        Assert.assertNotNull(group.syncId);
        Assert.assertNull(group.localId);
        Assert.assertEquals(TEST_GROUP_TITLE, group.title);
        Assert.assertEquals(TabGroupColorId.RED, group.color);
        Assert.assertEquals(3, group.savedTabs.size());
        Assert.assertEquals("creator_cache_guid", group.creatorCacheGuid);
        Assert.assertEquals("last_updater_cache_guid", group.lastUpdaterCacheGuid);
        Assert.assertNotNull(group.archivalTimeMs);

        SavedTabGroupTab tab1 = group.savedTabs.get(0);
        Assert.assertNotNull(tab1.syncId);
        Assert.assertNull(tab1.localId);
        Assert.assertEquals(group.syncId, tab1.syncGroupId);
        Assert.assertEquals("Google", tab1.title);
        Assert.assertEquals(new GURL("www.google.com"), tab1.url);
        Assert.assertEquals(Integer.valueOf(0), tab1.position);

        SavedTabGroupTab tab3 = group.savedTabs.get(2);
        Assert.assertNotNull(tab3.syncId);
        Assert.assertEquals(Integer.valueOf(9), tab3.localId);
        Assert.assertEquals(group.syncId, tab3.syncGroupId);
        Assert.assertEquals(Integer.valueOf(2), tab3.position);
        Assert.assertEquals(TEST_TAB_TITLE, tab3.title);
        Assert.assertEquals(new GURL(null), tab3.url);
        Assert.assertEquals("creator_cache_guid", tab3.creatorCacheGuid);
        Assert.assertEquals("last_updater_cache_guid", tab3.lastUpdaterCacheGuid);
    }

    @CalledByNative
    public void testOnTabGroupAdded() {
        verify(mObserver).onTabGroupAdded(mTabGroupCaptor.capture(), eq(TriggerSource.REMOTE));
        SavedTabGroup group = mTabGroupCaptor.getValue();
        Assert.assertEquals(new String(TEST_GROUP_TITLE), group.title);
        Assert.assertEquals(TabGroupColorId.BLUE, group.color);
    }

    @CalledByNative
    public void testOnTabGroupUpdated() {
        verify(mObserver).onTabGroupUpdated(mTabGroupCaptor.capture(), eq(TriggerSource.REMOTE));
        SavedTabGroup group = mTabGroupCaptor.getValue();
        Assert.assertEquals(new String(TEST_GROUP_TITLE), group.title);
        Assert.assertEquals(TabGroupColorId.BLUE, group.color);
    }

    @CalledByNative
    public void testOnTabGroupRemoved() {
        verify(mObserver).onTabGroupRemoved(eq(LOCAL_TAB_GROUP_ID_1), eq(TriggerSource.REMOTE));
        verify(mObserver).onTabGroupRemoved(anyString(), eq(TriggerSource.REMOTE));
    }

    @CalledByNative
    public void testOnTabGroupLocalIdChanged() {
        verify(mObserver).onTabGroupLocalIdChanged(anyString(), eq(LOCAL_TAB_GROUP_ID_1));
    }

    @CalledByNative
    public void testAddGroup() {
        SavedTabGroup group = new SavedTabGroup();
        group.localId = LOCAL_TAB_GROUP_ID_1;
        group.title = TEST_GROUP_TITLE;
        group.color = TabGroupColorId.GREEN;
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.localId = LOCAL_TAB_ID_1;
        tab1.url = new GURL(TEST_URL);
        tab1.title = TEST_TAB_TITLE;
        group.savedTabs.add(tab1);
        mService.addGroup(group);
    }

    @CalledByNative
    public void testRemoveGroupByLocalId() {
        mService.removeGroup(LOCAL_TAB_GROUP_ID_1);
    }

    @CalledByNative
    public void testRemoveGroupBySyncId(String uuid) {
        mService.removeGroup(uuid);
    }

    @CalledByNative
    public void testUpdateVisualData() {
        mService.updateVisualData(LOCAL_TAB_GROUP_ID_1, TEST_GROUP_TITLE_2, TabGroupColorId.GREEN);
    }

    @CalledByNative
    public void testMakeTabGroupShared(String collaborationId) {
        mService.makeTabGroupShared(
                LOCAL_TAB_GROUP_ID_1, collaborationId, /* tabGroupSharingCallback= */ null);
    }

    @CalledByNative
    public void testAboutToUnShareTabGroup() {
        mService.aboutToUnShareTabGroup(LOCAL_TAB_GROUP_ID_1, null);
    }

    @CalledByNative
    public void testOnTabGroupUnShareComplete() {
        mService.onTabGroupUnShareComplete(LOCAL_TAB_GROUP_ID_1, true);
    }

    @CalledByNative
    public void testAddTab() {
        mService.addTab(
                LOCAL_TAB_GROUP_ID_1,
                LOCAL_TAB_ID_1,
                TEST_TAB_TITLE,
                new GURL(TEST_URL),
                TAB_POSITION);
        mService.addTab(
                LOCAL_TAB_GROUP_ID_1, LOCAL_TAB_ID_2, TEST_TAB_TITLE, new GURL(TEST_URL), -1);
    }

    @CalledByNative
    public void testUpdateTab() {
        mService.updateTab(
                LOCAL_TAB_GROUP_ID_1,
                LOCAL_TAB_ID_1,
                TEST_TAB_TITLE,
                new GURL(TEST_URL),
                TAB_POSITION);
        mService.updateTab(LOCAL_TAB_GROUP_ID_1, 4, TEST_TAB_TITLE, new GURL(TEST_URL), -1);
    }

    @CalledByNative
    public void testRemoveTab() {
        mService.removeTab(LOCAL_TAB_GROUP_ID_1, LOCAL_TAB_ID_1);
    }

    @CalledByNative
    public void testMoveTab() {
        mService.moveTab(LOCAL_TAB_GROUP_ID_1, LOCAL_TAB_ID_1, TAB_POSITION);
    }

    @CalledByNative
    public void testGetAllGroups() {
        String[] groups = mService.getAllGroupIds();
        Assert.assertEquals(1, groups.length);
    }

    @CalledByNative
    public void testGetGroupBySyncId(String uuid1, String uuid2) {
        SavedTabGroup group = mService.getGroup(uuid1);
        Assert.assertNotNull(group);

        group = mService.getGroup(uuid2);
        Assert.assertNull(group);
    }

    @CalledByNative
    public void testGetGroupByLocalId(LocalTabGroupId localId1, LocalTabGroupId localId2) {
        SavedTabGroup group = mService.getGroup(localId1);
        Assert.assertNotNull(group);

        group = mService.getGroup(localId2);
        Assert.assertNull(group);
    }

    @CalledByNative
    public void testGetDeletedGroupIds() {
        List<LocalTabGroupId> groupIds = mService.getDeletedGroupIds();
        Assert.assertEquals(1, groupIds.size());
    }

    @CalledByNative
    public void testUpdateLocalTabGroupMapping(String syncId, LocalTabGroupId localId) {
        mService.updateLocalTabGroupMapping(syncId, localId, OpeningSource.AUTO_OPENED_FROM_SYNC);
    }

    @CalledByNative
    public void testRemoveLocalTabGroupMapping(LocalTabGroupId localId) {
        mService.removeLocalTabGroupMapping(localId, ClosingSource.DELETED_BY_USER);
    }

    @CalledByNative
    public void testUpdateLocalTabId(
            LocalTabGroupId localTabGroupId, String syncTabId, int localTabId) {
        mService.updateLocalTabId(localTabGroupId, syncTabId, localTabId);
    }

    @CalledByNative
    public void testOnTabSelected(
            LocalTabGroupId localTabGroupId, int localTabId, String tabTitle) {
        mService.onTabSelected(localTabGroupId, localTabId, tabTitle);
    }

    @CalledByNative
    public void testUpdateArchivalStatus(String uuid, boolean archivalStatus) {
        mService.updateArchivalStatus(uuid, archivalStatus);
    }

    @CalledByNative
    public void testIsInitialized() {
        Assert.assertTrue(mService.getVersioningMessageController().isInitialized());
    }

    @CalledByNative
    public void testShouldShowMessageUi() {
        Assert.assertTrue(
                mService.getVersioningMessageController()
                        .shouldShowMessageUi(MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE));
        Assert.assertFalse(
                mService.getVersioningMessageController()
                        .shouldShowMessageUi(MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE));
        Assert.assertTrue(
                mService.getVersioningMessageController()
                        .shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE));
    }

    @CalledByNative
    public void testShouldShowMessageUiAsync() {
        mCallback = mock(Callback.class);
        mService.getVersioningMessageController()
                .shouldShowMessageUiAsync(
                        MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE, mCallback);
        verify(mCallback).onResult(true);
    }

    @CalledByNative
    public void testOnMessageUiShown() {
        mService.getVersioningMessageController()
                .onMessageUiShown(MessageType.VERSION_OUT_OF_DATE_INSTANT_MESSAGE);
    }

    @CalledByNative
    public void testOnMessageUiDismissed() {
        mService.getVersioningMessageController()
                .onMessageUiDismissed(MessageType.VERSION_OUT_OF_DATE_PERSISTENT_MESSAGE);
    }
}
