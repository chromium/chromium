// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for {@link DeviceLockCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeviceLockCoordinatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private MockDelegate mMockDelegate;

    private Context mContext;

    @Before
    public void setUpTest() {
        mMockDelegate = Mockito.mock(MockDelegate.class);
        mContext = ContextUtils.getApplicationContext();
    }

    // TODO(crbug.com/1432028): Add more tests, particularly render tests, once flow is finalized
    @Test
    @SmallTest
    public void testDeviceLockCoordinator_simpleTest() {
        DeviceLockCoordinator deviceLockCoordinator =
                new DeviceLockCoordinator(true, mMockDelegate, null, mContext, null);
        assertNotEquals(deviceLockCoordinator, null);
        verify(mMockDelegate, times(1)).setView(any());

        deviceLockCoordinator.destroy();
    }

    private class MockDelegate implements DeviceLockCoordinator.Delegate {
        @Override
        public void setView(View view) {}

        @Override
        public void onDeviceLockReady() {}

        @Override
        public void onDeviceLockRefused() {}
    }
}
