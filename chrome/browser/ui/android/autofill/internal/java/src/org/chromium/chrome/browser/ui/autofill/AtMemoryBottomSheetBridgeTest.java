// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.autofill;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
public class AtMemoryBottomSheetBridgeTest {
    private static final long NATIVE_BRIDGE = 100L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AtMemoryBottomSheetBridge.Natives mNativeMock;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;

    private AtMemoryBottomSheetBridge mBridge;

    @Before
    public void setUp() throws Exception {
        AtMemoryBottomSheetBridgeJni.setInstanceForTesting(mNativeMock);
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(context));
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);

        mBridge = AtMemoryBottomSheetBridge.create(NATIVE_BRIDGE, mWindowAndroid);
        assertNotNull(mBridge);
    }

    @Test
    @SmallTest
    public void testOnDismissedNotCalledAfterDestroy() {
        mBridge.destroy();

        mBridge.onDismissed();

        verify(mNativeMock, never()).onDismissed(NATIVE_BRIDGE);
    }
}
