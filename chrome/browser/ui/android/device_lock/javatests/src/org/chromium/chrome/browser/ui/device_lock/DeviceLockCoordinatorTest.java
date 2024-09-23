// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link DeviceLockCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeviceLockCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private DeviceLockCoordinator.Delegate mMockDelegate;
    @Mock private Activity mActivity;

    @Before
    public void setUpTest() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.setFinishActivity(true);

        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        when(mMockDelegate.getSource()).thenReturn(DeviceLockActivityLauncher.Source.AUTOFILL);
    }

    @After
    public void tearDown() throws Exception {
        // Since the activity is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mActivity != null) {
            ApplicationTestUtils.finishActivity(mActivity);
        }
    }

    // TODO(crbug.com/40902690): Add more tests, particularly render tests, once flow is finalized
    @Test
    @SmallTest
    public void testDeviceLockCoordinator_simpleTest() {
        DeviceLockCoordinator deviceLockCoordinator =
                new DeviceLockCoordinator(
                        mMockDelegate, null, (ReauthenticatorBridge) null, mActivity, null);
        assertNotEquals(deviceLockCoordinator, null);
        verify(mMockDelegate, times(1)).setView(any());

        deviceLockCoordinator.destroy();
    }
}
