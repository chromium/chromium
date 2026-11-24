// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link OmniboxIconBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OmniboxIconBridgeUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private OmniboxIconBridge.Natives mNatives;
    @Mock private Runnable mOnIconsChanged;

    private OmniboxIconBridge mBridge;

    @Before
    public void setUp() {
        OmniboxIconBridgeJni.setInstanceForTesting(mNatives);
        doReturn(JniUniquePtr.createForTesting(1L)).when(mNatives).init(any());
        mBridge = new OmniboxIconBridge(mOnIconsChanged);
    }

    @After
    public void tearDown() {
        mBridge.destroy();
    }

    @Test
    public void onIconsChanged_runsCallback() {
        mBridge.onIconsChanged();

        verify(mOnIconsChanged).run();
    }
}
