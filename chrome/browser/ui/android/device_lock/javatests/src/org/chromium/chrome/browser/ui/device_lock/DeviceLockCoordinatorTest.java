// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for {@link DeviceLockCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeviceLockCoordinatorTest {
    private Context mContext;

    @Before
    public void setUpTest() {
        mContext = ContextUtils.getApplicationContext();
    }

    // TODO(crbug.com/1432028): Add more tests, particularly render tests, once flow is finalized
    @Test
    @SmallTest
    public void testDeviceLockCoordinator_simpleTest() {
        DeviceLockCoordinator deviceLockCoordinator =
                new DeviceLockCoordinator(true, new MockDelegate(), mContext);
        assertNotEquals(deviceLockCoordinator, null);

        deviceLockCoordinator.destroy();
    }

    private class MockDelegate implements DeviceLockCoordinator.Delegate {
        @Override
        public void onDeviceLockReady() {}

        @Override
        public void onDeviceLockRefused() {}
    }
}
