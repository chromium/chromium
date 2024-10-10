// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.tab_group_sync.messaging;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;
import org.mockito.ArgumentCaptor;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

import java.util.List;

/** A companion object to the native MessagingBackendServiceBridgeTest. */
@JNINamespace("tab_groups::messaging")
public class MessagingBackendServiceBridgeUnitTestCompanion {
    // The service instance we're testing.
    private MessagingBackendService mService;

    private MessagingBackendService.PersistentMessageObserver mObserver =
            mock(MessagingBackendService.PersistentMessageObserver.class);
    private MessagingBackendService.InstantMessageDelegate mInstantMessageDelegate =
            mock(MessagingBackendService.InstantMessageDelegate.class);

    private ArgumentCaptor<InstantMessage> mInstantMessageCaptor =
            ArgumentCaptor.forClass(InstantMessage.class);
    private ArgumentCaptor<Callback> mInstantMessageCallbackCaptor =
            ArgumentCaptor.forClass(Callback.class);

    @CalledByNative
    private MessagingBackendServiceBridgeUnitTestCompanion(MessagingBackendService service) {
        mService = service;
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private boolean isInitialized() {
        return mService.isInitialized();
    }

    @CalledByNative
    private void addPersistentMessageObserver() {
        mService.addPersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void removePersistentMessageObserver() {
        mService.removePersistentMessageObserver(mObserver);
    }

    @CalledByNative
    private void verifyOnInitializedCalled(int times) {
        verify(mObserver, times(times)).onMessagingBackendServiceInitialized();
    }

    @CalledByNative
    private void setInstantMessageDelegate() {
        mService.setInstantMessageDelegate(mInstantMessageDelegate);
    }

    @CalledByNative
    private void verifyInstantMessage() {
        verify(mInstantMessageDelegate)
                .displayInstantaneousMessage(
                        mInstantMessageCaptor.capture(), mInstantMessageCallbackCaptor.capture());
        InstantMessage message = mInstantMessageCaptor.getValue();
        Assert.assertEquals(InstantNotificationLevel.SYSTEM, message.level);
        Assert.assertEquals(InstantNotificationType.CONFLICT_TAB_REMOVED, message.type);
        Assert.assertEquals(UserAction.TAB_REMOVED, message.action);
    }

    @CalledByNative
    private void invokeInstantMessageSuccessCallback(boolean success) {
        mInstantMessageCallbackCaptor.getValue().onResult(success);
    }

    @CalledByNative
    private void invokeGetActivityLogAndVerify() {
        ActivityLogQueryParams queryParams = new ActivityLogQueryParams();
        queryParams.collaborationId = "collaboration1";
        List<ActivityLogItem> logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(2, logItems.size());

        Assert.assertEquals(UserAction.TAB_NAVIGATED, logItems.get(0).userActionType);
        Assert.assertEquals("title 1", logItems.get(0).titleText);
        Assert.assertEquals("description 1", logItems.get(0).descriptionText);
        Assert.assertEquals("timestamp 1", logItems.get(0).timestampText);

        Assert.assertEquals(UserAction.COLLABORATION_USER_JOINED, logItems.get(1).userActionType);
        Assert.assertEquals("title 2", logItems.get(1).titleText);
        Assert.assertEquals("description 2", logItems.get(1).descriptionText);
        Assert.assertEquals("timestamp 2", logItems.get(1).timestampText);

        queryParams.collaborationId = "collaboration2";
        logItems = mService.getActivityLog(queryParams);
        Assert.assertEquals(0, logItems.size());
    }
}
