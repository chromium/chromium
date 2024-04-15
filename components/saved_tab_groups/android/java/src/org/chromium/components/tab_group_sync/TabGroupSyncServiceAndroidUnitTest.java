// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync;

import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.os.Looper;
import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;
import org.mockito.ArgumentCaptor;

import org.chromium.base.ThreadUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Unit test to verify JNI conversions required for {@link TabGroupSyncServiceImpl}. This includes
 * various tests for conversion of tab group and tab IDs, SavedTabGroup and SavedTabGroupTab, and
 * mutation and observation method calls for the service. See native unit test:
 * (components/saved_tab_groups/android/tab_group_sync_service_android_unittest.cc).
 */
@JNINamespace("tab_groups")
public class TabGroupSyncServiceAndroidUnitTest {
    private TabGroupSyncService mService;
    private TabGroupSyncService.Observer mObserver;
    private ArgumentCaptor<SavedTabGroup> mTabGroupCaptor =
            ArgumentCaptor.forClass(SavedTabGroup.class);

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
    public void testSavedTabGroupJavaConversion(SavedTabGroup group) {
        Assert.assertNotNull(group);
        Assert.assertNotNull(group.syncId);
        Assert.assertNull(group.localId);
        Assert.assertEquals("Some Title", group.title);
        Assert.assertEquals(TabGroupColorId.RED, group.color);
        Assert.assertEquals(3, group.savedTabs.size());

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
        Assert.assertEquals("Tab title", tab3.title);
        Assert.assertEquals(new GURL(null), tab3.url);
    }

    @CalledByNative
    public void testOnTabGroupAdded() {
        verify(mObserver).onTabGroupAdded(mTabGroupCaptor.capture());
        SavedTabGroup group = mTabGroupCaptor.getValue();
        Assert.assertEquals(new String("Test Group"), group.title);
        Assert.assertEquals(TabGroupColorId.BLUE, group.color);
    }

    @CalledByNative
    public void testOnTabGroupUpdated() {
        verify(mObserver).onTabGroupAdded(mTabGroupCaptor.capture());
        SavedTabGroup group = mTabGroupCaptor.getValue();
        Assert.assertEquals(new String("Test Group"), group.title);
        Assert.assertEquals(TabGroupColorId.BLUE, group.color);
    }

    @CalledByNative
    public void testOnTabGroupRemoved() {
        verify(mObserver).onTabGroupRemoved(eq(4));
    }

    @CalledByNative
    public void testCreateGroup() {
        String uuid = mService.createGroup(4);
        Assert.assertFalse(TextUtils.isEmpty(uuid));
    }

    @CalledByNative
    public void testRemoveGroup() {
        mService.removeGroup(4);
    }

    @CalledByNative
    public void testUpdateVisualData() {
        mService.updateVisualData(4, "Updated Title", TabGroupColorId.GREEN);
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
    public void testUpdateLocalTabGroupId(String syncId, int localId) {
        mService.updateLocalTabGroupId(syncId, localId);
    }

    @CalledByNative
    public void testUpdateLocalTabId(int localTabGroupId, String syncTabId, int localTabId) {
        mService.updateLocalTabId(localTabGroupId, syncTabId, localTabId);
    }
}
