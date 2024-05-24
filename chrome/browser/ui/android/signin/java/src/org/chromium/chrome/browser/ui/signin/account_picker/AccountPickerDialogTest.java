// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.signin.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.IOException;

/** Instrumentation tests for account picker dialog. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AccountPickerDialogTest extends BlankUiTestActivityTestCase {
    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private AccountPickerCoordinator.Listener mListenerMock;

    private final String mFullName1 = "Test Account1";

    private final String mAccountName1 = "test.account1@gmail.com";

    private final String mAccountName2 = "test.account2@gmail.com";

    private AccountPickerDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(mAccountName1, mFullName1, null, null);
        mAccountManagerTestRule.addAccount(mAccountName2, "", null, null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new AccountPickerDialogCoordinator(
                                    getActivity(),
                                    mListenerMock,
                                    new ModalDialogManager(
                                            new AppModalPresenter(getActivity()),
                                            ModalDialogType.APP));
                });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialog);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testTitle() {
        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testAddAccount() {
        onView(withText(R.string.signin_add_account_to_device)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).addAccount();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectDefaultAccount() {
        onView(withText(mAccountName1)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(mFullName1)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onAccountSelected(mAccountName1);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSelectNonDefaultAccount() {
        onView(withText(mAccountName2)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onAccountSelected(mAccountName2);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testAccountPickerDialogView() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(), "account_picker_dialog");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testAccountPickerDialogView_replaceSyncWithSigninPromosEnabled()
            throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(),
                "account_picker_dialog_replace_sync_with_signin_promos_enabled");
    }
}
