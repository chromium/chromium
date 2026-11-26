// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMDriverJni;

import java.util.ArrayDeque;
import java.util.Queue;

/** Unit tests for {@link InstanceIDBridge} and {@link FakeInstanceIDWithSubtype}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstanceIDBridgeTest {
    private static final long NATIVE_BRIDGE_PTR = 1234L;
    private static final String SUBTYPE = "com.google.chrome.sync.invalidations";
    private static final String AUTHORIZED_ENTITY = "361488507004";
    private static final String SCOPE = "GCM";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private InstanceIDBridge.Natives mBridgeJni;
    @Mock private GCMDriver mMockGcmDriver;
    @Mock private GCMDriver.Natives mGcmDriverJni;

    @Before
    public void setUp() {
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(true);
        InstanceIDBridgeJni.setInstanceForTesting(mBridgeJni);
        GCMDriverJni.setInstanceForTesting(mGcmDriverJni);
    }

    @After
    public void tearDown() {
        FakeInstanceIDWithSubtype.clearDataAndSetEnabled(false);
    }

    @Test
    public void testDeleteInstanceIDAndGetTokenSequencedInOrder() {
        InstanceIDBridge bridge = InstanceIDBridge.create(NATIVE_BRIDGE_PTR, SUBTYPE);

        bridge.getToken(/* requestId= */ 1, AUTHORIZED_ENTITY, SCOPE, /* flags= */ 0);
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<String> tokenCaptor = ArgumentCaptor.forClass(String.class);
        verify(mBridgeJni).didGetToken(eq(NATIVE_BRIDGE_PTR), eq(1), tokenCaptor.capture());
        String token1 = tokenCaptor.getValue();
        assertFalse(token1.isEmpty());
        assertEquals(
                AUTHORIZED_ENTITY,
                FakeInstanceIDWithSubtype.getAuthorizedEntityForSubtype(SUBTYPE));

        // Override the pre-native thread pool executor with an explicit queue so we can verify
        // that SequencedTaskRunner only submits getToken() after deleteInstanceID() finishes.
        Queue<Runnable> queuedTasks = new ArrayDeque<>();
        PostTask.setPrenativeThreadPoolExecutorForTesting(queuedTasks::add);

        bridge.deleteInstanceID(/* requestId= */ 2);
        bridge.getToken(/* requestId= */ 3, AUTHORIZED_ENTITY, SCOPE, /* flags= */ 0);
        assertEquals(1, queuedTasks.size());

        queuedTasks.remove().run();
        assertFalse(InstanceIDWithSubtype.sSubtypeInstances.containsKey(SUBTYPE));
        assertEquals(1, queuedTasks.size());

        queuedTasks.remove().run();
        assertTrue(queuedTasks.isEmpty());
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<String> newTokenCaptor = ArgumentCaptor.forClass(String.class);
        InOrder inOrder = inOrder(mBridgeJni);
        inOrder.verify(mBridgeJni).didDeleteID(NATIVE_BRIDGE_PTR, 2, true);
        inOrder.verify(mBridgeJni)
                .didGetToken(eq(NATIVE_BRIDGE_PTR), eq(3), newTokenCaptor.capture());
        String token2 = newTokenCaptor.getValue();
        assertFalse(token2.isEmpty());
        assertNotEquals(token1, token2);
        assertEquals(
                AUTHORIZED_ENTITY,
                FakeInstanceIDWithSubtype.getAuthorizedEntityForSubtype(SUBTYPE));
    }

    @Test
    public void testDispatchMessageForTokenAndRecreateAfterDeleteInstanceID() {
        InstanceIDBridge bridge = InstanceIDBridge.create(NATIVE_BRIDGE_PTR, SUBTYPE);

        bridge.getToken(/* requestId= */ 1, AUTHORIZED_ENTITY, SCOPE, /* flags= */ 0);
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<String> tokenCaptor = ArgumentCaptor.forClass(String.class);
        verify(mBridgeJni).didGetToken(eq(NATIVE_BRIDGE_PTR), eq(1), tokenCaptor.capture());
        String token1 = tokenCaptor.getValue();
        assertFalse(token1.isEmpty());

        byte[] payload1 = new byte[] {1, 2, 3};
        // Without an active GCMDriver instance, dispatching returns false.
        assertFalse(FakeInstanceIDWithSubtype.dispatchMessageForToken(token1, payload1));

        GCMDriver.setInstanceForTesting(mMockGcmDriver);
        assertTrue(FakeInstanceIDWithSubtype.dispatchMessageForToken(token1, payload1));
        verify(mGcmDriverJni)
                .onMessageReceived(
                        anyLong(),
                        eq(SUBTYPE),
                        eq(AUTHORIZED_ENTITY),
                        any(),
                        any(),
                        eq(payload1),
                        any());

        bridge.deleteInstanceID(/* requestId= */ 2);
        bridge.getToken(/* requestId= */ 3, AUTHORIZED_ENTITY, SCOPE, /* flags= */ 0);
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<String> newTokenCaptor = ArgumentCaptor.forClass(String.class);
        verify(mBridgeJni).didGetToken(eq(NATIVE_BRIDGE_PTR), eq(3), newTokenCaptor.capture());
        String token2 = newTokenCaptor.getValue();
        assertNotEquals(token1, token2);

        byte[] payload2 = new byte[] {4, 5, 6};
        assertFalse(FakeInstanceIDWithSubtype.dispatchMessageForToken(token1, payload2));
        assertTrue(FakeInstanceIDWithSubtype.dispatchMessageForToken(token2, payload2));
        verify(mGcmDriverJni)
                .onMessageReceived(
                        anyLong(),
                        eq(SUBTYPE),
                        eq(AUTHORIZED_ENTITY),
                        any(),
                        any(),
                        eq(payload2),
                        any());
    }
}
