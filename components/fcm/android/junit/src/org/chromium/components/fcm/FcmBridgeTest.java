// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.fcm;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.LinkedHashMap;
import java.util.Map;

/** Unit tests for {@link FcmBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FcmBridgeTest {
    private static final long NATIVE_DRIVER_PTR = 12345L;
    private static final String TEST_MESSAGE_ID = "test_message_id";

    @Mock private FcmBridge.Natives mMockNative;
    @Mock private FcmManager mMockFcmManager;

    private FcmBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        FcmBridgeJni.setInstanceForTesting(mMockNative);
        FcmManager.setInstanceForTesting(mMockFcmManager);
        mBridge = FcmBridge.create(NATIVE_DRIVER_PTR);
    }

    @After
    public void tearDown() {
        FcmBridgeJni.setInstanceForTesting(null);
        FcmManager.setInstanceForTesting(null);
        if (FcmBridge.getInstance() != null) {
            FcmBridge.getInstance().destroy();
        }
    }

    @Test
    public void testOnMessageReceived() {
        assertNotNull(FcmBridge.getInstance());

        Map<String, String> data = new LinkedHashMap<>();
        data.put("subtype", "test_app");
        data.put("key1", "value1");
        byte[] rawData = new byte[] {1, 2, 3};

        mBridge.onMessageReceived(TEST_MESSAGE_ID, data, rawData);

        verify(mMockNative)
                .onMessageReceived(
                        NATIVE_DRIVER_PTR,
                        TEST_MESSAGE_ID,
                        new String[] {"subtype", "test_app", "key1", "value1"},
                        rawData);
    }

    @Test
    public void testOnMessagesDeleted() {
        mBridge.onMessagesDeleted();

        verify(mMockNative).onMessagesDeleted(NATIVE_DRIVER_PTR);
    }

    @Test
    public void testFetchInstallationId() {
        doAnswer(
                        invocation -> {
                            Callback<String> callback = invocation.getArgument(0);
                            callback.onResult("test_installation_id");
                            return null;
                        })
                .when(mMockFcmManager)
                .fetchInstallationId(org.mockito.ArgumentMatchers.any());

        mBridge.fetchInstallationId();
        // Wait for the background worker task and the subsequent UI-thread callback to finish.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        verify(mMockNative).onInstallationIdRefreshed(NATIVE_DRIVER_PTR, "test_installation_id");
    }
}
