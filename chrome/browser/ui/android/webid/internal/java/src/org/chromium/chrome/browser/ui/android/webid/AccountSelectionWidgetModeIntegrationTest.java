// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.graphics.Color;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.RelyingPartyData;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.url.GURL;

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
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            Arrays.asList(mNewBobWithAddAccount),
                            Arrays.asList(mIdpDataWithAddAccount),
                            /* newAccounts= */ Collections.emptyList());
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        View contentView = mBottomSheetController.getCurrentSheetContent().getContentView();
        assertNotNull(contentView);

        // Add account secondary button does not exist on passive mode, despite IDP supporting add
        // account.
        onView(withId(R.id.account_selection_add_account_btn)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testErrorDialogBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN,
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.BACK_PRESS);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    @Test
    @MediumTest
    public void testErrorDialogSwipeDismissesAndCallsCallback() {
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showErrorDialog(
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN,
                            TOKEN_ERROR);
                });
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.FULL);
        BottomSheetTestSupport sheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        runOnUiThreadBlocking(
                () -> {
                    sheetSupport.suppressSheet(BottomSheetController.StateChangeReason.SWIPE);
                });
        waitForEvent(mMockBridge).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
        verify(mMockBridge, never()).onAccountSelected(any());
    }

    /**
     * Tests that with `FedCmIdpLinks`, continuing with a new account on the passive mode sheet
     * replaces it with the active mode sheet on the request permission dialog, which shows the
     * identity provider's links.
     */
    @Test
    @MediumTest
    public void testContinueWithNewAccountSwitchesToActiveSheet() {
        BottomSheetContent activeSheet = switchToActiveSheet();

        runOnUiThreadBlocking(
                () -> {
                    assertTrue(activeSheet.getSheetType().isModal());
                    assertEquals(
                            HeaderType.REQUEST_PERMISSION_MODAL,
                            mAccountSelection.getMediator().getHeaderType());
                    assertTrue(
                            activeSheet
                                    .getContentView()
                                    .findViewById(R.id.idp_links_row)
                                    .isShown());
                });
        verify(mMockBridge, never()).onAccountSelected(any());
        verify(mMockBridge, never()).onDismissed(anyInt());
    }

    /**
     * Tests that showing the failure dialog after switching to the active mode sheet replaces it
     * with a new passive mode sheet.
     */
    @Test
    @MediumTest
    public void testFailureDialogAfterSwitchShowsPassiveSheet() {
        BottomSheetContent activeSheet = switchToActiveSheet();

        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showFailureDialog(
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            TEST_ETLD_PLUS_ONE_2,
                            IDP_METADATA,
                            RpContext.SIGN_IN);
                });
        pollUiThread(
                () -> {
                    BottomSheetContent sheet = mBottomSheetController.getCurrentSheetContent();
                    return sheet != null
                            && sheet != activeSheet
                            && !sheet.getSheetType().isModal()
                            && getBottomSheetState() == SheetState.FULL;
                });
        runOnUiThreadBlocking(
                () -> {
                    BottomSheetContent passiveSheet =
                            mBottomSheetController.getCurrentSheetContent();
                    assertEquals(
                            HeaderType.SIGN_IN_TO_IDP_STATIC,
                            mAccountSelection.getMediator().getHeaderType());
                    assertTrue(
                            passiveSheet.getContentView().findViewById(R.id.idp_signin).isShown());
                });
        verify(mMockBridge, never()).onDismissed(anyInt());
    }

    /**
     * Tests that with `FedCmIdpLinks` and `FedCmActiveModeMultipleIdentityProviders`, selecting a
     * new account in the passive mode account chooser for multiple identity providers replaces it
     * with the active mode sheet on the request permission dialog, and that back there shows the
     * active mode account chooser with the accounts of both identity providers.
     */
    @Test
    @MediumTest
    public void testSelectingNewAccountWithMultipleIdpsSwitchesToActiveSheet() {
        FeatureOverrides.overrideFlag(ChromeFeatureList.FED_CM_IDP_LINKS, true);
        FeatureOverrides.overrideFlag(
                ContentFeatures.FED_CM_ACTIVE_MODE_MULTIPLE_IDENTITY_PROVIDERS, true);
        BottomSheetContent passiveSheet = selectNewAccountWithMultipleIdps();

        // See `switchToActiveSheet()` for why either settled state is accepted.
        pollUiThread(
                () -> {
                    BottomSheetContent sheet = mBottomSheetController.getCurrentSheetContent();
                    @SheetState int state = getBottomSheetState();
                    return sheet != null
                            && sheet != passiveSheet
                            && (state == SheetState.HALF || state == SheetState.FULL);
                });
        BottomSheetContent activeSheet =
                runOnUiThreadBlocking(
                        () -> {
                            BottomSheetContent sheet =
                                    mBottomSheetController.getCurrentSheetContent();
                            assertTrue(sheet.getSheetType().isModal());
                            assertEquals(
                                    HeaderType.REQUEST_PERMISSION_MODAL,
                                    mAccountSelection.getMediator().getHeaderType());
                            assertTrue(
                                    sheet.getContentView()
                                            .findViewById(R.id.idp_links_row)
                                            .isShown());
                            return sheet;
                        });

        Espresso.pressBack();

        pollUiThread(() -> mAccountSelection.getMediator().getHeaderType() == HeaderType.SIGN_IN);
        runOnUiThreadBlocking(
                () -> {
                    assertSame(activeSheet, mBottomSheetController.getCurrentSheetContent());
                    RecyclerView accountsList =
                            activeSheet.getContentView().findViewById(R.id.sheet_item_list);
                    assertEquals(2, accountsList.getAdapter().getItemCount());
                });
        verify(mMockBridge, never()).onAccountSelected(any());
        verify(mMockBridge, never()).onDismissed(anyInt());
    }

    /**
     * Tests that with `FedCmIdpLinks` but without `FedCmActiveModeMultipleIdentityProviders`,
     * selecting a new account in the passive mode account chooser for multiple identity providers
     * keeps asking for permission in the passive mode sheet.
     */
    @Test
    @MediumTest
    public void testSelectingNewAccountWithMultipleIdpsWithoutFlagStaysInPassiveSheet() {
        // `FedCmActiveModeMultipleIdentityProviders` has no test value, so the passive mode sheet
        // reads it from native through the content feature map. It's disabled by default.
        FeatureOverrides.overrideFlag(ChromeFeatureList.FED_CM_IDP_LINKS, true);
        BottomSheetContent passiveSheet = selectNewAccountWithMultipleIdps();

        pollUiThread(
                () ->
                        passiveSheet
                                .getContentView()
                                .findViewById(R.id.user_data_sharing_consent)
                                .isShown());
        runOnUiThreadBlocking(
                () -> {
                    assertSame(passiveSheet, mBottomSheetController.getCurrentSheetContent());
                    assertFalse(passiveSheet.getSheetType().isModal());
                });
        verify(mMockBridge, never()).onAccountSelected(any());
        verify(mMockBridge, never()).onDismissed(anyInt());
    }

    /**
     * Shows the passive mode account chooser for `mNewBob` and a returning account of a second
     * identity provider, selects `mNewBob`, and returns the passive mode sheet.
     */
    private BottomSheetContent selectNewAccountWithMultipleIdps() {
        Account accountForSecondIdp = createReturningAccountForSecondIdp();
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            Arrays.asList(mNewBob, accountForSecondIdp),
                            Arrays.asList(mIdpData, accountForSecondIdp.getIdentityProviderData()),
                            /* newAccounts= */ Collections.emptyList());
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        // With multiple identity providers, the passive mode sheet can open at half height.
        pollUiThread(
                () -> {
                    @SheetState int state = getBottomSheetState();
                    return state == SheetState.HALF || state == SheetState.FULL;
                });
        return runOnUiThreadBlocking(
                () -> {
                    BottomSheetContent sheet = mBottomSheetController.getCurrentSheetContent();
                    RecyclerView accountsList =
                            sheet.getContentView().findViewById(R.id.sheet_item_list);
                    // The first row is the one of `mNewBob`.
                    accountsList.getChildAt(0).performClick();
                    return sheet;
                });
    }

    /**
     * Returns a returning account of an identity provider other than `mIdpData`, for tests with
     * multiple identity providers.
     */
    private Account createReturningAccountForSecondIdp() {
        IdentityProviderData secondIdpData =
                new IdentityProviderData(
                        "idp2.test",
                        new IdentityProviderMetadata(
                                /* brandTextColor= */ Color.WHITE,
                                /* brandBackgroundColor= */ Color.BLACK,
                                /* brandIconBitmap= */ null,
                                /* configUrl= */ new GURL("https://idp2.test/fedcm.json"),
                                /* loginUrl= */ new GURL("https://idp2.test/login"),
                                /* accountManagementUrl= */ GURL.emptyGURL(),
                                /* privacyPolicyUrl= */ GURL.emptyGURL(),
                                /* termsOfServiceUrl= */ GURL.emptyGURL(),
                                /* showUseDifferentAccountButton= */ false),
                        mClientIdMetadata,
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);
        return new Account(
                "Cat",
                "cat@idp2.test",
                "Cat Doe",
                "Cat",
                /* secondaryDescription= */ null,
                /* pictureBitmap= */ null,
                /* circledBadgedPictureBitmap= */ null,
                /* isIdpClaimedSignIn= */ true,
                /* isBrowserTrustedSignIn= */ true,
                /* isFilteredOut= */ false,
                /* fields= */ new int[0],
                secondIdpData);
    }

    /**
     * Shows the passive mode sheet for a new account, continues with it so that the active mode
     * sheet replaces it, and returns the settled active mode sheet.
     */
    private BottomSheetContent switchToActiveSheet() {
        // The flag is first read when the accounts are shown.
        FeatureOverrides.overrideFlag(ChromeFeatureList.FED_CM_IDP_LINKS, true);
        runOnUiThreadBlocking(
                () -> {
                    mAccountSelection.showAccounts(
                            new RelyingPartyData(
                                    EXAMPLE_ETLD_PLUS_ONE,
                                    /* iframeForDisplay= */ "",
                                    /* rpIcon= */ null),
                            Arrays.asList(mNewBob),
                            Arrays.asList(mIdpData),
                            /* newAccounts= */ Collections.emptyList());
                    mAccountSelection.getMediator().setComponentShowTime(-1000);
                });
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        BottomSheetContent passiveSheet =
                runOnUiThreadBlocking(
                        () -> {
                            BottomSheetContent sheet =
                                    mBottomSheetController.getCurrentSheetContent();
                            // The active mode sheet shows the disclosure text instead.
                            assertFalse(
                                    sheet.getContentView()
                                            .findViewById(R.id.user_data_sharing_consent)
                                            .isShown());
                            sheet.getContentView()
                                    .findViewById(R.id.account_selection_continue_btn)
                                    .performClick();
                            return sheet;
                        });

        // The active mode sheet opens at half height, or at full height if the request permission
        // dialog with IdP links leaves less than the minimum distance to full height on a shorter
        // screen. Wait for either settled state rather than `isSheetOpen()`, which turns true while
        // the sheet is still `SheetState.SCROLLING`.
        pollUiThread(
                () -> {
                    BottomSheetContent sheet = mBottomSheetController.getCurrentSheetContent();
                    @SheetState int state = getBottomSheetState();
                    return sheet != null
                            && sheet != passiveSheet
                            && (state == SheetState.HALF || state == SheetState.FULL);
                });
        return runOnUiThreadBlocking(() -> mBottomSheetController.getCurrentSheetContent());
    }
}
