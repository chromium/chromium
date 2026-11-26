// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_preferences.synced_set_up;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.jni_zero.JniUniquePtr;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;

import java.util.Map;

/** Unit tests for {@link PrefToValueMapBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PrefToValueMapBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PrefToValueMapBridge.Natives mNativesMock;

    private long mNextNativePtr = 1L;

    @Before
    public void setUp() {
        PrefToValueMapBridgeJni.setInstanceForTesting(mNativesMock);
        when(mNativesMock.init(any()))
                .thenAnswer(inv -> JniUniquePtr.createForTesting(mNextNativePtr++));
    }

    @Test
    public void testPopulateAndDestroyIdempotent() {
        PrefToValueMapBridge bridge = new PrefToValueMapBridge();
        assertNotNull(bridge.getNativeBridgePtr());

        bridge.putStringValue("str_key", "hello");
        bridge.putIntValue("int_key", 42);
        bridge.putBooleanValue("bool_key", true);

        assertEquals(
                Map.of("str_key", "hello", "int_key", 42, "bool_key", true),
                bridge.getPrefValueMap());

        bridge.destroy();
        bridge.destroy();
    }

    @Test
    public void testGetNativeBridgePtrAfterDestroyThrows() {
        PrefToValueMapBridge bridge = new PrefToValueMapBridge();
        bridge.destroy();

        assertThrows(IllegalStateException.class, bridge::getNativeBridgePtr);
    }

    @Test
    public void testLeakDetectedWhenNotDestroyed() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        // BaseRobolectricTestRule.setUp() initializes LibraryLoader, whose constructor registers
        // LifetimeAssert's safe-pointer tracker factory once per Robolectric sandbox.
        PrefToValueMapBridge bridge = new PrefToValueMapBridge();
        try {
            RuntimeException exception =
                    assertThrows(
                            RuntimeException.class,
                            LifetimeAssert::assertAllInstancesDestroyedForTesting);
            assertTrue(exception.getMessage().contains("was not destroyed"));
        } finally {
            bridge.destroy();
        }
    }
}
