// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HEADER_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collections;

/** Controller tests verify that the Account Selection Active Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeControllerTest extends AccountSelectionJUnitTestBase {
    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.ACTIVE;
        super.setUp();
    }

    @Test
    public void testShowVerifySheetExplicitSignin() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    Arrays.asList(mNewUserAccount),
                    Arrays.asList(mIdpData),
                    /* newAccounts= */ Collections.EMPTY_LIST);
            mMediator.showVerifySheet(mAnaAccount);

            // There is no account shown in the verify sheet on active mode.
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertTrue(mModel.get(ItemProperties.SPINNER_ENABLED));
            assertTrue(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));
        }
    }

    @Test
    public void testShowLoadingDialogAutoReauthn() {
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne1, RpContext.SIGN_IN);
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            mMediator.showVerifyingDialog(mAnaAccount, /* isAutoReauthn= */ true);

            // There is no account shown on the loading dialog in active mode.
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.LOADING, mModel.get(ItemProperties.HEADER).get(TYPE));
            assertTrue(mModel.get(ItemProperties.SPINNER_ENABLED));
            assertTrue(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));
        }
    }

    @Test
    public void testShowLoadingDialog() {
        // Button flow can be triggered regardless of the requestShowContent result.
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne1, RpContext.SIGN_IN);
        assertEquals(0, mSheetAccountItems.size());
        assertEquals(HeaderType.LOADING, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockDelegate, never()).onAccountsDisplayed();

        // For loading dialog, we expect dragbar handle + header + spinner.
        assertEquals(3, countAllItems());
        assertTrue(mModel.get(ItemProperties.SPINNER_ENABLED));
        assertTrue(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));

        // Switching to accounts dialog should disable the spinner.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));

        // For accounts dialog, we expect dragbar handle + header + two accounts.
        assertEquals(4, countAllItems());
        assertFalse(mModel.get(ItemProperties.SPINNER_ENABLED));
        assertTrue(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));
    }

    @Test
    public void testShowRequestPermissionDialog() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.showRequestPermissionModalSheet(mNewUserAccount);

        // For request permission dialog, we expect drag handlebar + header + account chip +
        // disclosure text + continue button.
        assertEquals(5, countAllItems());

        // There is no sheet account items because the account is shown in an account chip instead.
        assertEquals(0, mSheetAccountItems.size());
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertTrue(containsItemOfType(mModel, ItemProperties.ACCOUNT_CHIP));
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    @Test
    public void testShowAccountsUsesRpIcon() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.EMPTY_LIST);

        assertNotNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }

    @Test
    public void testBrandIconNotPresent() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccountWithoutBrandIcons),
                Arrays.asList(mIdpDataWithoutIcons),
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        // Unlike passive mode, brand icons should not be available because we do not show any
        // placeholder icon.
        assertNull(headerModel.get(HEADER_ICON));
        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }

    @Test
    public void testNewAccountsIdpSingleNewAccountShowsRequestPermissionDialog() {
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne2, RpContext.SIGN_IN);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleNewAccount);

        // Request permission modal dialog is NOT skipped for a single newly signed-in new account.
        // Since
        // this is a new account and request permission is true, we need to show the request
        // permission modal dialog to gather permission from the user.
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    @Test
    public void testNewAccountsIdpRequestPermissionFalseShowsAccountChooserDialog() {
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne2, RpContext.SIGN_IN);
        mIdpData.setDisclosureFields(new int[0]);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleNewAccount);

        // Account chooser dialog is shown for a single newly signed-in new account where request
        // permission is false. Since this is a new account and request permission is false, we need
        // to show UI without disclosure text so we show the account chooser.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(1, mSheetAccountItems.size());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
    }

    @Test
    public void testNewAccountsIdpSingleReturningAccountShowsAccountChooserDialog() {
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne2, RpContext.SIGN_IN);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleReturningAccount);

        // Account chooser dialog is shown for a single newly signed-in returning account. Although
        // this is a returning account, we cannot skip directly to signing in because we have to
        // show browser UI in the flow so we show the account chooser.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(1, mSheetAccountItems.size());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
    }

    @Test
    public void testShowErrorDialogClickGotIt() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne2, RpContext.SIGN_IN);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleReturningAccount);
        mMediator.showErrorDialog(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                mIdpMetadata,
                RpContext.SIGN_IN,
                mTokenErrorEmptyUrl);

        assertEquals(
                ModalDialogManager.ModalDialogType.APP, mMockModalDialogManager.getDialogType());
        final PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertEquals(
                mContext.getString(R.string.signin_error_dialog_got_it_button),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE,
                model.get(ModalDialogProperties.BUTTON_STYLES));

        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertNull(mMockModalDialogManager.getDialogModel());
        assertEquals(-1, mMockModalDialogManager.getDialogType());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowErrorDialogClickMoreDetails() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showLoadingDialog(mTestEtldPlusOne, mTestEtldPlusOne1, RpContext.SIGN_IN);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleReturningAccount);
        mMediator.showErrorDialog(
                mTestEtldPlusOne, mTestEtldPlusOne2, mIdpMetadata, RpContext.SIGN_IN, mTokenError);

        assertEquals(
                ModalDialogManager.ModalDialogType.APP, mMockModalDialogManager.getDialogType());
        final PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertEquals(
                mContext.getString(R.string.signin_error_dialog_got_it_button),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.signin_error_dialog_more_details_button),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                model.get(ModalDialogProperties.BUTTON_STYLES));

        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.NEGATIVE);
        assertNull(mMockModalDialogManager.getDialogModel());
        assertEquals(-1, mMockModalDialogManager.getDialogType());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.MORE_DETAILS_BUTTON);
        assertTrue(mMediator.wasDismissed());
    }
}
