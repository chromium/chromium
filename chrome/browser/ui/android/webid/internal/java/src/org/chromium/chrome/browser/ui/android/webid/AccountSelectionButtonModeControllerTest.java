// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HEADER_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IS_MULTI_IDP_REQUEST;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IS_SHOWING_MULTIPLE_IDPS;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_CONTEXT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.content.Intent;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.RelyingPartyData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Controller tests verify that the Account Selection Active Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionButtonModeControllerTest extends AccountSelectionJUnitTestBase {
    @Mock private WebContents mMockWebContents;
    @Mock private UkmRecorder.Natives mUkmRecorderJniMock;

    @Before
    @Override
    public void setUp() {
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);
        when(mTab.getWebContents()).thenReturn(mMockWebContents);
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
                    new RelyingPartyData(
                            mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                    Arrays.asList(mNewUserAccount),
                    Arrays.asList(mIdpData),
                    /* newAccounts= */ Collections.emptyList());
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
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne1,
                RpContext.SIGN_IN);
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            mMediator.showVerifyingDialog(
                    new RelyingPartyData(
                            mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                    mAnaAccount,
                    /* isAutoReauthn= */ true);

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
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne1,
                RpContext.SIGN_IN);
        assertEquals(0, mSheetAccountItems.size());
        assertEquals(HeaderType.LOADING, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockDelegate, never()).onAccountsDisplayed();

        // For loading dialog, we expect dragbar handle + header + spinner.
        assertEquals(3, countAllItems());
        assertTrue(mModel.get(ItemProperties.SPINNER_ENABLED));
        assertTrue(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));

        // Switching to accounts dialog should disable the spinner.
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());
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
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());
        // The request permission dialog is opened from the single-account chooser.
        clickContinueButton();

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

    /**
     * Tests that the single-account chooser shows the continue button without disclosure text, even
     * when the use-a-different-account button is not shown. Regression test for
     * crbug.com/579901494.
     */
    @Test
    public void testSingleNewAccountShowsAccountChooserWithoutDisclosureText() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());

        assertSingleAccountChooserWithoutDisclosureText(mNewUserAccount);
    }

    /**
     * Tests that clicking continue on the single-account chooser opens the request permission
     * dialog, and that pressing back on it returns to the chooser, which still shows the continue
     * button. Regression test for crbug.com/579901494.
     */
    @Test
    public void testBackPressOnRequestPermissionDialogShowsAccountChooser() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());
        clickContinueButton();
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        assertTrue(mBottomSheetContent.handleBackPress());

        assertSingleAccountChooserWithoutDisclosureText(mNewUserAccount);
    }

    @Test
    public void testShowAccountsUsesRpIcon() {
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());

        assertNotNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }

    @Test
    public void testBrandIconNotPresent() {
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccountWithoutBrandIcons),
                Arrays.asList(mIdpDataWithoutIcons),
                /* newAccounts= */ Collections.emptyList());

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        // Unlike passive mode, brand icons should not be available because we do not show any
        // placeholder icon.
        assertNull(headerModel.get(HEADER_ICON));
        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
    }

    @Test
    public void testNewAccountsIdpSingleNewAccountShowsRequestPermissionDialog() {
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                RpContext.SIGN_IN);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
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
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                RpContext.SIGN_IN);
        mIdpData.setDisclosureFields(new int[0]);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(),
                Arrays.asList(mIdpData),
                Arrays.asList(mNewUserAccountWithoutFields));

        // Account chooser dialog is shown for a single newly signed-in new account where request
        // permission is false. Since this is a new account and request permission is false, we need
        // to show UI without disclosure text so we show the account chooser.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(1, mSheetAccountItems.size());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
    }

    @Test
    public void testNewAccountsIdpSingleReturningAccountShowsAccountChooserDialog() {
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                RpContext.SIGN_IN);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
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
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                RpContext.SIGN_IN);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleReturningAccount);
        mMediator.showErrorDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
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
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne1,
                RpContext.SIGN_IN);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(),
                Arrays.asList(mIdpData),
                mNewAccountsSingleReturningAccount);
        mMediator.showErrorDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                mIdpMetadata,
                RpContext.SIGN_IN,
                mTokenError);

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

    @Test
    public void testMultiIdpAccountChooser() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccount, mDanAccount),
                Arrays.asList(mIdpData, mIdpData2),
                /* newAccounts= */ Collections.emptyList());

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertTrue(headerModel.get(IS_SHOWING_MULTIPLE_IDPS));
        assertNull(headerModel.get(IDP_FOR_DISPLAY));
        // The view binder shows a fallback icon.
        assertNull(headerModel.get(HEADER_ICON));
        assertTrue(headerModel.get(IS_MULTI_IDP_REQUEST));
        verify(mMockDelegate).onAccountsDisplayed();

        // `mIdpData2` supports use a different account, but there is no row for it.
        assertEquals(2, mSheetAccountItems.size());
        assertTrue(mSheetAccountItems.get(0).model.get(AccountProperties.SHOW_IDP));
        assertTrue(mSheetAccountItems.get(1).model.get(AccountProperties.SHOW_IDP));
        assertFalse(containsItemOfType(mModel, ItemProperties.ADD_ACCOUNT_BUTTON));
        assertFalse(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    @Test
    public void testMultiIdpSingleAccountNotAutoselected() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData, mIdpData2),
                /* newAccounts= */ Collections.emptyList());

        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(1, mSheetAccountItems.size());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
        assertFalse(containsItemOfType(mModel, ItemProperties.ADD_ACCOUNT_BUTTON));
        assertFalse(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    @Test
    public void testMultiIdpSomeAccountsFilteredOutShowsAccountChooser() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccount, mFilteredOutAccountForIdp2),
                Arrays.asList(mIdpData, mIdpData2),
                /* newAccounts= */ Collections.emptyList());

        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertTrue(mModel.get(ItemProperties.HEADER).get(IS_SHOWING_MULTIPLE_IDPS));
        // TODO(crbug.com/581411351): `mIdpData2` has no row to use a different account until
        // there is an IDP picker.
        assertEquals(2, mSheetAccountItems.size());
        // The filtered out account is not clickable.
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
        assertNull(mSheetAccountItems.get(1).model.get(AccountProperties.ON_CLICK_LISTENER));
    }

    @Test
    public void testMultiIdpRequestPermissionShowsSelectedAccountIdp() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccountWithoutBrandIcons, mNewUserAccountForIdp2),
                Arrays.asList(mIdpDataWithoutIcons, mIdpData2),
                /* newAccounts= */ Collections.emptyList());
        mMediator.setComponentShowTime(-1000);

        mSheetAccountItems
                .get(1)
                .model
                .get(AccountProperties.ON_CLICK_LISTENER)
                .onResult(new ButtonData(mNewUserAccountForIdp2, /* idpMetadata= */ null));

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, headerModel.get(TYPE));
        assertFalse(headerModel.get(IS_SHOWING_MULTIPLE_IDPS));
        assertEquals(mTestEtldPlusOne1, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(HEADER_ICON));
        assertNotNull(headerModel.get(RP_BRAND_ICON));
    }

    @Test
    public void testMultiIdpLoadingDialog() {
        // Content passes an empty IDP when there are multiple IDPs.
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                /* idpForDisplay= */ "",
                RpContext.SIGN_IN);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.LOADING, headerModel.get(TYPE));
        assertEquals("", headerModel.get(IDP_FOR_DISPLAY));
        assertTrue(headerModel.get(IS_SHOWING_MULTIPLE_IDPS));
        assertTrue(mModel.get(ItemProperties.SPINNER_ENABLED));

        TextView title = mContentView.findViewById(R.id.header_title);
        TextView subtitle = mContentView.findViewById(R.id.header_subtitle);
        assertEquals("Sign in to " + mTestEtldPlusOne, title.getText().toString());
        assertEquals(View.GONE, subtitle.getVisibility());
    }

    @Test
    public void testMultiIdpLoadingThenSingleIdpNewAccountShowsIdpOnPermissionSheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        // Content passes an empty IDP when there are multiple IDPs.
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                /* idpForDisplay= */ "",
                RpContext.SIGN_IN);
        // Only one IDP is left, and the user has signed in to it with a new account.
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                mNewAccountsSingleNewAccount);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, headerModel.get(TYPE));
        assertEquals(mIdpData.getIdpForDisplay(), headerModel.get(IDP_FOR_DISPLAY));
    }

    @Test
    public void testSingleIdpSingleAccountWithUseDifferentAccountShowsButtons() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccountWithUseDifferentAccount),
                Arrays.asList(mIdpDataWithUseDifferentAccount),
                /* newAccounts= */ Collections.emptyList());

        // The single account chooser keeps the use a different account secondary button.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertFalse(mModel.get(ItemProperties.HEADER).get(IS_SHOWING_MULTIPLE_IDPS));
        assertEquals(1, mSheetAccountItems.size());
        assertTrue(containsItemOfType(mModel, ItemProperties.ADD_ACCOUNT_BUTTON));
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    @Test
    public void testLoadingModalDialogClosedByUserDismissesRequest() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showLoadingDialog(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                mTestEtldPlusOne2,
                RpContext.SIGN_IN);
        mMediator.onModalDialogOpened();

        mMediator.onModalDialogClosedByUser();

        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    @Test
    public void testIdpPickerShowsIdpRowsThatLogIn() {
        openIdpPickerFromAccountChooser();

        PropertyModel idpPickerHeader = mModel.get(ItemProperties.HEADER);
        assertTrue(idpPickerHeader.get(IS_SHOWING_MULTIPLE_IDPS));
        assertTrue(idpPickerHeader.get(IS_MULTI_IDP_REQUEST));
        assertNull(idpPickerHeader.get(IDP_FOR_DISPLAY));
        assertNull(idpPickerHeader.get(HEADER_ICON));
        assertFalse(mModel.get(ItemProperties.SPINNER_ENABLED));
        assertFalse(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        assertFalse(containsItemOfType(mModel, ItemProperties.ADD_ACCOUNT_BUTTON));

        assertEquals(2, mSheetAccountItems.size());
        assertEquals(AccountSelectionProperties.ITEM_TYPE_IDP, mSheetAccountItems.get(0).type);
        assertEquals(
                mIdpData,
                mSheetAccountItems.get(0).model.get(IdpProperties.PROPERTIES).mIdentityProvider);
        assertEquals(AccountSelectionProperties.ITEM_TYPE_IDP, mSheetAccountItems.get(1).type);
        assertEquals(
                mIdpData2,
                mSheetAccountItems.get(1).model.get(IdpProperties.PROPERTIES).mIdentityProvider);

        clickIdpRow(/* itemIndex= */ 0);
        verify(mMockDelegate).onLoginToIdP(mTestConfigUrl, mTestLoginUrl);
    }

    @Test
    public void testIdpPickerClickRowRespectsComponentShowTime() {
        showMultiIdpAccountChooser(Arrays.asList(mAnaAccount, mDanAccount));
        mMediator.setComponentShowTime(-1000);
        assertTrue(mMediator.showIdpPicker());

        // Without advancing past the show-time threshold after opening the picker, clicking an IDP
        // row is ignored.
        IdpProperties.Properties idpProps =
                mSheetAccountItems.get(0).model.get(IdpProperties.PROPERTIES);
        idpProps.mOnClickListener.onResult(
                new ButtonData(/* account= */ null, idpProps.mIdentityProvider.getIdpMetadata()));

        verify(mMockDelegate, never()).onLoginToIdP(any(), any());
    }

    @Test
    public void testIdpPickerNotifiesOnAccountsDisplayedWhenFirstShown() {
        when(mTab.isHidden()).thenReturn(true);
        showMultiIdpAccountChooser(Arrays.asList(mAnaAccount, mDanAccount));
        verify(mMockDelegate, never()).onAccountsDisplayed();

        when(mTab.isHidden()).thenReturn(false);
        assertTrue(mMediator.showIdpPicker());
        verify(mMockDelegate).onAccountsDisplayed();
    }

    @Test
    public void testIdpPickerDismissesWhenShowContentFails() {
        showMultiIdpAccountChooser(Arrays.asList(mAnaAccount, mDanAccount));
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);

        assertFalse(mMediator.showIdpPicker());
        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    @Test
    public void testIdpPickerClosedSheetTypeRecordsAccountSelection() {
        openIdpPickerFromAccountChooser();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Blink.FedCm.ClosedSheetType.Android", SheetType.ACCOUNT_SELECTION);
        hideBottomSheet(BottomSheetController.StateChangeReason.CLOSE_BUTTON);

        histogramWatcher.assertExpected();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.CLOSE_BUTTON);
    }

    @Test
    public void testIdpPickerDismissedBySwipe() {
        openIdpPickerFromAccountChooser();
        hideBottomSheet(BottomSheetController.StateChangeReason.SWIPE);

        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.SWIPE);
    }

    @Test
    public void testIdpPickerDismissedByTapScrim() {
        openIdpPickerFromAccountChooser();
        hideBottomSheet(BottomSheetController.StateChangeReason.TAP_SCRIM);

        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);
    }

    private void hideBottomSheet(@BottomSheetController.StateChangeReason int reason) {
        when(mMockBottomSheetController.getCurrentSheetContent()).thenReturn(mBottomSheetContent);
        ArgumentCaptor<BottomSheetObserver> observerCaptor =
                ArgumentCaptor.forClass(BottomSheetObserver.class);
        verify(mMockBottomSheetController).addObserver(observerCaptor.capture());
        observerCaptor
                .getValue()
                .onSheetStateChanged(BottomSheetController.SheetState.HIDDEN, reason);
    }

    private void showMultiIdpAccountChooser(List<Account> accounts) {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                accounts,
                Arrays.asList(mIdpData, mIdpData2),
                /* newAccounts= */ Collections.emptyList());
    }

    private void openIdpPickerFromAccountChooser() {
        showMultiIdpAccountChooser(Arrays.asList(mAnaAccount, mDanAccount));
        assertTrue(mMediator.showIdpPicker());
        assertEquals(HeaderType.IDP_PICKER, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    /** Clicks an IDP row on the IDP picker. */
    private void clickIdpRow(int itemIndex) {
        mMediator.setComponentShowTime(-1000);
        IdpProperties.Properties idpProps =
                mSheetAccountItems.get(itemIndex).model.get(IdpProperties.PROPERTIES);
        idpProps.mOnClickListener.onResult(
                new ButtonData(/* account= */ null, idpProps.mIdentityProvider.getIdpMetadata()));
    }

    /**
     * Asserts that the sheet shows the single-account chooser for {@code account}, with a clickable
     * account row and a continue button that selects it, without disclosure text.
     */
    private void assertSingleAccountChooserWithoutDisclosureText(Account account) {
        // Drag handlebar + header + account + continue button.
        assertEquals(4, countAllItems());
        assertEquals(1, mSheetAccountItems.size());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        assertEquals(
                account,
                mModel.get(ItemProperties.CONTINUE_BUTTON)
                        .get(ContinueButtonProperties.PROPERTIES)
                        .mAccount);
    }

    /**
     * Tests that with `FedCmIdpLinks` enabled, the request permission dialog has the identity
     * provider's links.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testRequestPermissionDialogHasIdpLinks() {
        DataSharingConsentProperties.Properties properties = showRequestPermissionDialog();
        assertEquals(mTestIdpAccountManagementUrl, properties.mIdpAccountManagementUrl);
        assertEquals(mTestIdpPrivacyPolicyUrl, properties.mIdpPrivacyPolicyUrl);
        assertEquals(mTestIdpTermsOfServiceUrl, properties.mIdpTermsOfServiceUrl);
    }

    /**
     * Tests that without `FedCmIdpLinks`, the request permission dialog doesn't have the identity
     * provider's links, even though the identity provider has them.
     */
    @Test
    public void testRequestPermissionDialogHasNoIdpLinksWithoutFlag() {
        DataSharingConsentProperties.Properties properties = showRequestPermissionDialog();
        assertTrue(properties.mIdpAccountManagementUrl.isEmpty());
        assertTrue(properties.mIdpPrivacyPolicyUrl.isEmpty());
        assertTrue(properties.mIdpTermsOfServiceUrl.isEmpty());
    }

    /**
     * Tests that each of the identity provider's links opens its URL in a Custom Tab, without
     * recording the RP's link metrics.
     */
    @Test
    // Opening a Custom Tab checks `CCTDontOverrideIntentMimeType`, so it needs a test value.
    @EnableFeatures({
        ChromeFeatureList.FED_CM_IDP_LINKS,
        ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE
    })
    public void testClickIdpLinks() {
        DataSharingConsentProperties.Properties properties = showRequestPermissionDialog();
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Blink.FedCm.SignUp.PrivacyPolicyClicked")
                        .expectNoRecords("Blink.FedCm.SignUp.TermsOfServiceClicked")
                        .build();
        for (GURL url :
                Arrays.asList(
                        properties.mIdpAccountManagementUrl,
                        properties.mIdpPrivacyPolicyUrl,
                        properties.mIdpTermsOfServiceUrl)) {
            properties.mIdpLinkClickCallback.accept(mContext, url);
            Intent intent = shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
            assertEquals(url.getSpec(), intent.getDataString());
        }
        histogramWatcher.assertExpected();
    }

    /**
     * Shows the request permission dialog for a new account, through the single-account chooser,
     * and returns the properties of its disclosure text.
     */
    private DataSharingConsentProperties.Properties showRequestPermissionDialog() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());
        clickContinueButton();
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
        return mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                .get(DataSharingConsentProperties.PROPERTIES);
    }

    /**
     * Tests that when the active mode sheet replaces the passive mode one, it shows the request
     * permission dialog with the identity provider's links, titled with the account's identity
     * provider, without notifying the delegate again that the accounts were displayed.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testShowRequestPermissionFromPassiveSheet() {
        mIdpData.setRpContext(RpContext.SIGN_UP);
        showRequestPermissionFromPassiveSheet(Arrays.asList(mNewUserAccount));

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertEquals(RpContext.SIGN_UP, (int) headerModel.get(RP_CONTEXT));
        assertEquals(
                mNewUserAccount,
                mModel.get(ItemProperties.ACCOUNT_CHIP).get(AccountProperties.ACCOUNT));
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        DataSharingConsentProperties.Properties properties =
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES);
        assertEquals(mTestIdpAccountManagementUrl, properties.mIdpAccountManagementUrl);
        assertEquals(mTestIdpPrivacyPolicyUrl, properties.mIdpPrivacyPolicyUrl);
        assertEquals(mTestIdpTermsOfServiceUrl, properties.mIdpTermsOfServiceUrl);
        verify(mMockDelegate, never()).onAccountsDisplayed();
    }

    /**
     * Tests that the request permission dialog shown for a passive mode request ignores a click
     * right after it is shown, e.g. the second tap of a double tap on the passive mode sheet.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testRequestPermissionFromPassiveSheetIgnoresEarlyClick() {
        showRequestPermissionFromPassiveSheet(Arrays.asList(mNewUserAccount));

        ContinueButtonProperties.Properties continueButton =
                mModel.get(ItemProperties.CONTINUE_BUTTON).get(ContinueButtonProperties.PROPERTIES);
        continueButton.mOnClickListener.onResult(
                new ButtonData(continueButton.mAccount, continueButton.mIdpMetadata));

        verify(mMockDelegate, never()).onAccountSelected(any());
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    /**
     * Tests that continuing on the request permission dialog shown for a passive mode request
     * selects the account and shows the verifying sheet.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testContinueOnRequestPermissionFromPassiveSheetShowsVerifySheet() {
        showRequestPermissionFromPassiveSheet(Arrays.asList(mNewUserAccount));

        clickContinueButton();

        verify(mMockDelegate).onAccountSelected(mNewUserAccount);
        assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    /**
     * Tests that back on the request permission dialog shown for a passive mode request shows the
     * active mode account chooser with all the accounts.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testBackOnRequestPermissionFromPassiveSheetShowsAccountChooser() {
        showRequestPermissionFromPassiveSheet(Arrays.asList(mAnaAccount, mNewUserAccount));

        assertTrue(mBottomSheetContent.handleBackPress());

        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(2, mSheetAccountItems.size());
        assertEquals(mAnaAccount, mSheetAccountItems.get(0).model.get(AccountProperties.ACCOUNT));
        assertEquals(
                mNewUserAccount, mSheetAccountItems.get(1).model.get(AccountProperties.ACCOUNT));
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        verify(mMockDelegate, never()).onAccountsDisplayed();
    }

    /**
     * Tests that the request permission dialog shown for a passive mode request with multiple
     * identity providers is titled with the selected account's identity provider and shows its RP
     * icon, and that back shows the active mode account chooser for multiple identity providers,
     * where selecting another identity provider's account titles the dialog with that one.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testRequestPermissionFromPassiveSheetWithMultipleIdps() {
        // `mIdpData2` offers to use a different account, which in passive mode means that it has a
        // filtered out account.
        showRequestPermissionFromPassiveSheet(
                Arrays.asList(mNewUserAccount, mNewUserAccountForIdp2, mFilteredOutAccountForIdp2),
                Arrays.asList(mIdpData, mIdpData2),
                mNewUserAccountForIdp2);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne1, headerModel.get(IDP_FOR_DISPLAY));
        assertSame(
                mIdpData2.getClientMetadata().getBrandIconBitmap(), headerModel.get(RP_BRAND_ICON));
        assertTrue(headerModel.get(IS_MULTI_IDP_REQUEST));

        assertTrue(mBottomSheetContent.handleBackPress());

        headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertTrue(headerModel.get(IS_SHOWING_MULTIPLE_IDPS));
        // One row per account.
        // TODO(crbug.com/581411351): `mIdpData2` has no row to use a different account until there
        // is an IDP picker.
        assertEquals(3, mSheetAccountItems.size());
        assertEquals(
                mNewUserAccount, mSheetAccountItems.get(0).model.get(AccountProperties.ACCOUNT));
        assertTrue(mSheetAccountItems.get(0).model.get(AccountProperties.SHOW_IDP));

        mMediator.setComponentShowTime(-1000);
        mSheetAccountItems
                .get(0)
                .model
                .get(AccountProperties.ON_CLICK_LISTENER)
                .onResult(new ButtonData(mNewUserAccount, /* idpMetadata= */ null));

        headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        verify(mMockDelegate, never()).onAccountSelected(any());
    }

    /**
     * Tests that continuing or dismissing on the request permission dialog shown for a passive mode
     * request doesn't record `Button.*` UMA or UKM metrics.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testContinueAndDismissAfterSwitchFromPassiveSheetRecordsNoButtonMetrics() {
        HistogramWatcher histogramWatcher = watchNoButtonMetrics();
        showRequestPermissionFromPassiveSheet(Arrays.asList(mNewUserAccount));

        clickContinueButton();
        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
        assertNoButtonUkmRecorded();
    }

    /**
     * Tests that pressing back on the request permission dialog shown for a passive mode request
     * and then selecting an account or dismissing the account chooser doesn't record `Button.*` UMA
     * or UKM metrics.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testBackAndAccountChooserAfterSwitchFromPassiveSheetRecordsNoButtonMetrics() {
        HistogramWatcher histogramWatcher = watchNoButtonMetrics();
        showRequestPermissionFromPassiveSheet(Arrays.asList(mAnaAccount, mNewUserAccount));

        assertTrue(mBottomSheetContent.handleBackPress());
        mMediator.setComponentShowTime(-1000);
        mSheetAccountItems
                .get(0)
                .model
                .get(AccountProperties.ON_CLICK_LISTENER)
                .onResult(new ButtonData(mAnaAccount, /* idpMetadata= */ null));
        mMediator.onDismissed(IdentityRequestDialogDismissReason.TAP_SCRIM);

        histogramWatcher.assertExpected();
        assertNoButtonUkmRecorded();
    }

    /**
     * Tests that showing accounts in the active mode sheet that replaced the passive mode one, e.g.
     * after logging in to the identity provider, doesn't record `Button.*` UMA or UKM metrics.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testShowAccountsAfterSwitchFromPassiveSheetRecordsNoButtonMetrics() {
        showRequestPermissionFromPassiveSheet(Arrays.asList(mNewUserAccount));
        HistogramWatcher histogramWatcher = watchNoButtonMetrics();

        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                mNewAccountsSingleNewAccount);
        assertEquals(
                HeaderType.REQUEST_PERMISSION_MODAL, mModel.get(ItemProperties.HEADER).get(TYPE));
        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);

        histogramWatcher.assertExpected();
        assertNoButtonUkmRecorded();
    }

    private HistogramWatcher watchNoButtonMetrics() {
        return HistogramWatcher.newBuilder()
                .expectNoRecords("Blink.FedCm.Button.AccountChooserResult")
                .expectNoRecords("Blink.FedCm.Button.DisclosureDialogResult")
                .expectNoRecords("Blink.FedCm.Button.LoadingDialogResult")
                .build();
    }

    private void assertNoButtonUkmRecorded() {
        verify(mUkmRecorderJniMock, never())
                .recordEventWithMultipleMetrics(
                        any(),
                        any(),
                        argThat(
                                metrics ->
                                        Arrays.stream(metrics)
                                                .anyMatch(m -> m.mName.startsWith("Button."))));
    }

    /**
     * Shows the request permission dialog for `mNewUserAccount`, as when the active mode sheet
     * replaces the passive mode one for a passive mode request with {@code accounts}.
     */
    private void showRequestPermissionFromPassiveSheet(List<Account> accounts) {
        showRequestPermissionFromPassiveSheet(accounts, Arrays.asList(mIdpData), mNewUserAccount);
    }

    /**
     * Shows the request permission dialog for {@code account}, as when the active mode sheet
     * replaces the passive mode one for a passive mode request with {@code accounts} and {@code
     * idpDataList}.
     */
    private void showRequestPermissionFromPassiveSheet(
            List<Account> accounts, List<IdentityProviderData> idpDataList, Account account) {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showRequestPermissionFromPassiveSheet(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                accounts,
                idpDataList,
                account);
    }
}
