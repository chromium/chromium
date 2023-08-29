// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/**
 * Tests for {@link MissingDeviceLockCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MissingDeviceLockCoordinatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;

    @Before
    public void setUpTest() {
        mActivity = Mockito.mock(Activity.class);
        mActivityTestRule.setFinishActivity(true);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.TAB);

        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
    }

    @After
    public void tearDown() throws Exception {
        // Since the activity is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mActivity != null) {
            ApplicationTestUtils.finishActivity(mActivity);
        }
    }

    @Test
    @SmallTest
    public void testMissingDeviceLockCoordinator_showAndHideDialog() throws InterruptedException {
        MissingDeviceLockCoordinator missingDeviceLockCoordinator =
                new MissingDeviceLockCoordinator(
                        (wipeAllData) -> {}, mActivity, mModalDialogManager);
        assertNotEquals(missingDeviceLockCoordinator, null);
        missingDeviceLockCoordinator.showDialog();
        assertTrue("The modal dialog should be showing.", mModalDialogManager.isShowing());
        missingDeviceLockCoordinator.hideDialog(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}
