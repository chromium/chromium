// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertNotNull;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.Arrays;
import java.util.Collections;

/**
 * Integration tests for the Account Selection Passive Mode component check that the calls to the
 * Account Selection API end up rendering a View.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountSelectionWidgetModeIntegrationTest extends AccountSelectionIntegrationTestBase {
    @Before
    @Override
    public void setUp() throws InterruptedException {
        mRpMode = RpMode.PASSIVE;
        super.setUp();
    }

    @Test
    @MediumTest
    public void testAddAccountDisabled() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            Arrays.asList(NEW_BOB),
                            mIdpDataWithAddAccount,
                            /* isAutoReauthn= */ false,
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Add account secondary button does not exist on passive mode, despite IDP supporting add
        // account.
        onView(withId(R.id.account_selection_add_account_btn)).check(doesNotExist());
    }
}
