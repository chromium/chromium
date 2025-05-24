// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.Arrays;
import java.util.Collections;

/**
 * Integration tests for the Account Selection Active Mode component check that the calls to the
 * Account Selection API end up rendering a View. This class is parameterized to run all tests for
 * each RP mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccountSelectionButtonModeIntegrationTest extends AccountSelectionIntegrationTestBase {
    @Before
    @Override
    public void setUp() throws InterruptedException {
        mRpMode = RpMode.ACTIVE;
        super.setUp();
    }

    @Test
    @MediumTest
    public void testAddReturningUserAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mAccountSelection.showAccounts(
                                        EXAMPLE_ETLD_PLUS_ONE,
                                        Arrays.asList(
                                                mNewBobWithAddAccount, mReturningAnaWithAddAccount),
                                        Arrays.asList(mIdpDataWithAddAccount),
                                        mNewAccountsReturningAna);
                                mAccountSelection.getMediator().setComponentShowTime(-1000);
                                return null;
                            }
                        })
                .when(mMockBridge)
                .onLoginToIdP(any(), any());

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        // Because of newAccounts and the account is a returning user, user is now signed in and
        // shown the verifying UI.
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testAddNewUserAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mAccountSelection.showAccounts(
                                        EXAMPLE_ETLD_PLUS_ONE,
                                        Arrays.asList(
                                                mNewBobWithAddAccount, mReturningAnaWithAddAccount),
                                        Arrays.asList(mIdpDataWithAddAccount),
                                        mNewAccountsNewBob);
                                mAccountSelection.getMediator().setComponentShowTime(-1000);
                                return null;
                            }
                        })
                .when(mMockBridge)
                .onLoginToIdP(any(), any());

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        // Because of newAccounts and the account is a non-returning user which requires us to
        // show disclosure text, the next dialog shown should be the request permission dialog with
        // only the newly signed-in account and the disclosure text shown.
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());
        onView(withId(R.id.account_selection_continue_btn)).check(matches(withText("Continue")));
        onView(withId(R.id.user_data_sharing_consent))
                .check(
                        matches(
                                withText(
                                        "To continue, two.com will share your name, email address,"
                                            + " and profile picture with this site. See this site's"
                                            + " privacy policy and terms of service.")));

        // Click "Continue" to proceed to the verifying UI.
        clickContinueButton();
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testBrowserTrustedLoginStatePrecedesLoginState() {
        Account account =
                new Account(
                        "Test",
                        "test@one.test",
                        "Test",
                        "Test",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        mIdpDataWithAddAccount);

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                mAccountSelection.showAccounts(
                                        EXAMPLE_ETLD_PLUS_ONE,
                                        Arrays.asList(account, mReturningAnaWithAddAccount),
                                        Arrays.asList(mIdpDataWithAddAccount),
                                        Arrays.asList(account));
                                mAccountSelection.getMediator().setComponentShowTime(-1000);
                                return null;
                            }
                        })
                .when(mMockBridge)
                .onLoginToIdP(any(), any());

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        // Because of newAccounts and the account's IDP claimed login state does not match the
        // account's browser trusted login state, we show the account chooser UI since browser
        // trusted login state takes precedence. We do not show the request permission UI because
        // the IDP claimed login state tells us to not show the disclosure text.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
        onView(withId(R.id.account_selection_continue_btn))
                .check(matches(withText("Continue as Test")));

        // Click "Continue" to proceed to the verifying UI.
        clickContinueButton();
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountForNewUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        // This should be the "multi-account chooser", so clicking an account should go
        // to the disclosure text screen.
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        clickFirstAccountInAccountsList();

        // Sheet should still be open.
        assertNotEquals(BottomSheetController.SheetState.HIDDEN, getBottomSheetState());
        onView(withId(R.id.account_selection_continue_btn)).check(matches(withText("Continue")));

        // Make sure we now show the disclosure text.
        TextView consent = contentView.findViewById(R.id.user_data_sharing_consent);
        if (consent == null) {
            throw new NoMatchingViewException.Builder()
                    .includeViewHierarchy(true)
                    .withRootView(contentView)
                    .build();
        }

        clickContinueButton();

        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testAccountChooserWithAddAccountReturningUser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));

        clickFirstAccountInAccountsList();

        // Because this is a returning account, we should immediately sign in now.
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testAddAccountIsSecondaryButtonForSingleAccount() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Check that only one item is in the accounts list, and the item is an account.
        RecyclerView accountsList = contentView.findViewById(R.id.sheet_item_list);
        assertEquals(1, accountsList.getChildCount());
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                accountsList.getAdapter().getItemViewType(0));

        // Check that secondary button is displayed, with the appropriate text.
        onView(withId(R.id.account_selection_add_account_btn))
                .check(matches(withText("Use a different account")));
    }

    @Test
    @MediumTest
    public void testAddAccountIsAccountRowForMultipleAccounts() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount, mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Check that three items are in the accounts list, the first two items are accounts and the
        // third/last item is an add account button.
        RecyclerView accountsList = contentView.findViewById(R.id.sheet_item_list);
        assertEquals(3, accountsList.getChildCount());
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                accountsList.getAdapter().getItemViewType(0));
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_ACCOUNT,
                accountsList.getAdapter().getItemViewType(1));
        assertEquals(
                AccountSelectionProperties.ITEM_TYPE_LOGIN,
                accountsList.getAdapter().getItemViewType(2));

        // Check that secondary button is NOT displayed.
        onView(withId(R.id.account_selection_add_account_btn)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testLoadingDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testLoadingDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testNewUserFlow() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account in the account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
        clickFirstAccountInAccountsList();

        // Click continue in the request permission dialog.
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());
        clickContinueButton();

        // User is now signed in and shown the verifying UI.
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testReturningUserFlow() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account account in the account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
        clickFirstAccountInAccountsList();

        // Because this is a returning account, user is now signed in and shown the verifying UI.
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testRpInApprovedClientsFlow() {
        mIdpDataWithAddAccount.setDisclosureFields(new int[0]);
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Click the first account account in the account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
        clickFirstAccountInAccountsList();

        // Because disclosureFields are empty, user is now signed in and shown the verifying UI.
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());
        verify(mMockBridge, never()).onDismissed(anyInt());
        verify(mMockBridge).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogBackShowsAccountChooser() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());

        // Press back from the request permission dialog, returning to the account chooser.
        Espresso.pressBack();
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());

        // Swipe to dismiss on request permission dialog.
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testAccountChooserToVerifyingDialogHeaderReused() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());
        String expectedTitle =
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString();
        String expectedSubtitle =
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString();
        onView(withId(R.id.header_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(not(isDisplayed())));
        onView(withId(R.id.arrow_range_icon)).check(matches(not(isDisplayed())));

        // Clicking an account should show the verifying dialog.
        clickFirstAccountInAccountsList();
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());
        assertEquals(
                expectedTitle,
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString());
        assertEquals(
                expectedSubtitle,
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString());
        onView(withId(R.id.header_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(not(isDisplayed())));
        onView(withId(R.id.arrow_range_icon)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testRequestPermissionDialogToVerifyingDialogHeaderReused() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Dialog is initially an account chooser.
        assertEquals(HeaderType.SIGN_IN, mAccountSelection.getMediator().getHeaderType());

        // Clicking an account should show the request permission dialog.
        clickFirstAccountInAccountsList();
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());
        String expectedTitle =
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString();
        String expectedSubtitle =
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString();
        onView(withId(R.id.header_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.arrow_range_icon)).check(matches(isDisplayed()));

        // Clicking continue should show the verifying dialog.
        clickContinueButton();
        assertEquals(HeaderType.VERIFY, mAccountSelection.getMediator().getHeaderType());
        assertEquals(
                expectedTitle,
                ((TextView) contentView.findViewById(R.id.header_title)).getText().toString());
        assertEquals(
                expectedSubtitle,
                ((TextView) contentView.findViewById(R.id.header_subtitle)).getText().toString());
        onView(withId(R.id.header_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.header_rp_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.arrow_range_icon)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAccountSelectionRecordsAccountChooserResultHistogram() {
        mIdpDataWithAddAccount.setDisclosureFields(new int[0]);
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult",
                        AccountChooserResult.ACCOUNT_ROW);

        clickFirstAccountInAccountsList();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testAddAccountRecordsAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult",
                        AccountChooserResult.USE_OTHER_ACCOUNT_BUTTON);

        // Click "Use a different account".
        runOnUiThreadBlocking(
                () -> {
                    contentView.findViewById(R.id.account_selection_add_account_btn).performClick();
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSwipeRecordsAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAna, mNewBob),
                            Arrays.asList(mIdpData),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.SWIPE);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPressBackAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.BACK_PRESS);

        Espresso.pressBack();
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTapScrimAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.TAP_SCRIM);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.forceClickOutsideTheSheet();
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTabClosedAccountChooserResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.AccountChooserResult", AccountChooserResult.TAB_CLOSED);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.NAVIGATION);
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testMaximizeSheet() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> sheetSupport.setSheetState(BottomSheetController.SheetState.FULL, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
    }

    @Test
    @MediumTest
    public void testErrorDialogDismissesCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            EXAMPLE_ETLD_PLUS_ONE,
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN,
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

        ModalDialogManager dialogManager =
                mActivityTestRule.getActivity().getModalDialogManagerSupplier().get();
        CriteriaHelper.pollUiThread(() -> dialogManager.isShowing());
        ThreadUtils.runOnUiThreadBlocking(
                () -> dialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN));

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testProceedRecordsLoadingDialogResultHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult", LoadingDialogResult.PROCEED);

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testProceedThroughPopupRecordsLoadingDialogResultHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult",
                        LoadingDialogResult.PROCEED_THROUGH_POPUP);

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                    mAccountSelection.getMediator().onModalDialogClosed();
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mReturningAnaWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSwipeRecordsLoadingDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult", LoadingDialogResult.SWIPE);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPressBackLoadingDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult", LoadingDialogResult.BACK_PRESS);

        Espresso.pressBack();
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTapScrimLoadingDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult", LoadingDialogResult.TAP_SCRIM);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.forceClickOutsideTheSheet();
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDestroyLoadingDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showLoadingDialog(
                            EXAMPLE_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, RpContext.SIGN_IN);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.LoadingDialogResult", LoadingDialogResult.DESTROY);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.NAVIGATION);
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testContinueRecordsDisclosureDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        clickFirstAccountInAccountsList();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.DisclosureDialogResult",
                        DisclosureDialogResult.CONTINUE);

        // Click continue in the request permission dialog.
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL,
                mAccountSelection.getMediator().getHeaderType());
        clickContinueButton();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSwipeRecordsDisclosureDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        clickFirstAccountInAccountsList();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.DisclosureDialogResult", DisclosureDialogResult.SWIPE);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testPressBackDisclosureDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        clickFirstAccountInAccountsList();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.DisclosureDialogResult",
                        DisclosureDialogResult.BACK_PRESS);

        Espresso.pressBack();

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testTapScrimDisclosureDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        clickFirstAccountInAccountsList();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.DisclosureDialogResult",
                        DisclosureDialogResult.TAP_SCRIM);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.forceClickOutsideTheSheet();
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);

        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testDestroyDisclosureDialogResultHistogram() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            EXAMPLE_ETLD_PLUS_ONE,
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.EMPTY_LIST);
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HALF);
        clickFirstAccountInAccountsList();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.Button.DisclosureDialogResult",
                        DisclosureDialogResult.DESTROY);

        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.NAVIGATION);
                });

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
    }

    private void clickFirstAccountInAccountsList() {
        runOnUiThreadBlocking(
                () -> {
                    ((RecyclerView)
                                    mBottomSheetController
                                            .getCurrentSheetContent()
                                            .getContentView()
                                            .findViewById(R.id.sheet_item_list))
                            .getChildAt(0)
                            .performClick();
                });
    }

    private void clickContinueButton() {
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController
                            .getCurrentSheetContent()
                            .getContentView()
                            .findViewById(R.id.account_selection_continue_btn)
                            .performClick();
                });
    }
}
