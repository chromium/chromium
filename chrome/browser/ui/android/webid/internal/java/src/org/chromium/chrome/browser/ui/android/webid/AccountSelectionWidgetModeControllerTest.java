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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HEADER_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_CONTEXT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_MODE;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Color;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.RelyingPartyData;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Controller tests verify that the Account Selection Passive Mode delegate modifies the model. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionWidgetModeControllerTest extends AccountSelectionJUnitTestBase {
    @Before
    @Override
    public void setUp() {
        mRpMode = RpMode.PASSIVE;
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

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
            assertFalse(mModel.get(ItemProperties.SPINNER_ENABLED));
            assertFalse(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));
        }
    }

    @Test
    public void testShowVerifySheetAutoReauthn() {
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mIdpData.setRpContext(rpContext);
            mMediator.showVerifyingDialog(
                    new RelyingPartyData(
                            mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                    mAnaAccount,
                    /* isAutoReauthn= */ true);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
            assertFalse(mModel.get(ItemProperties.SPINNER_ENABLED));
            assertFalse(mModel.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE));
        }
    }

    @Test
    public void testShowAccountsWithoutBrandIcons() {
        mMediator.showAccounts(
                new RelyingPartyData(
                        mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                Arrays.asList(mAnaAccountWithoutBrandIcons),
                Arrays.asList(mIdpDataWithoutIcons),
                /* newAccounts= */ Collections.emptyList());

        assertNull(mModel.get(ItemProperties.HEADER).get(RP_BRAND_ICON));
        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(HEADER_ICON));
    }

    @Test
    public void testShowErrorDialogClickGotIt() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    new RelyingPartyData(
                            mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                    mTestEtldPlusOne2,
                    mIdpMetadata,
                    rpContext,
                    mTokenErrorEmptyUrl);
            assertEquals(0, mSheetAccountItems.size());

            PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
            assertEquals(HeaderType.SIGN_IN_ERROR, headerModel.get(TYPE));
            assertNotNull(headerModel.get(HEADER_ICON));
            assertEquals(
                    "Header has incorrect IDP for display",
                    mTestEtldPlusOne2,
                    headerModel.get(IDP_FOR_DISPLAY));
            assertEquals(
                    "Header has incorrect RP for display",
                    mTestEtldPlusOne,
                    headerModel.get(RP_FOR_DISPLAY));
            assertNull(headerModel.get(RP_BRAND_ICON));
            assertEquals(
                    "Header has the incorrect RP context", rpContext, headerModel.get(RP_CONTEXT));
            assertEquals(
                    "Header has the incorrect RP mode",
                    (Integer) mRpMode,
                    headerModel.get(RP_MODE));

            verify(mMockDelegate, never()).onAccountsDisplayed();

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1", mTestEtldPlusOne2, errorProperties.mIdpForDisplay);
            assertEquals("Incorrect RP ETLD+1", mTestEtldPlusOne, errorProperties.mRpForDisplay);
            assertEquals("Incorrect token error", mTokenErrorEmptyUrl, errorProperties.mError);

            assertNotNull(
                    mModel.get(ItemProperties.CONTINUE_BUTTON)
                            .get(ContinueButtonProperties.PROPERTIES)
                            .mOnClickListener);
            assertNull(
                    mModel.get(ItemProperties.ERROR_TEXT)
                            .get(ErrorProperties.PROPERTIES)
                            .mMoreDetailsClickRunnable);

            // Do not let test inputs be ignored.
            mMediator.setComponentShowTime(-1000);
            mModel.get(ItemProperties.CONTINUE_BUTTON)
                    .get(ContinueButtonProperties.PROPERTIES)
                    .mOnClickListener
                    .onResult(new ButtonData(mAnaAccount, /* idpMetadata= */ mIdpMetadata));
            verify(mMockDelegate, times(++count))
                    .onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
            assertTrue(mMediator.wasDismissed());
            // Reset mediator after dismiss.
            resetMediator();
        }
    }

    @Test
    public void testShowErrorDialogClickMoreDetails() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    new RelyingPartyData(
                            mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null),
                    mTestEtldPlusOne2,
                    mIdpMetadata,
                    rpContext,
                    mTokenError);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.SIGN_IN_ERROR, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate, never()).onAccountsDisplayed();

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1", mTestEtldPlusOne2, errorProperties.mIdpForDisplay);
            assertEquals("Incorrect RP ETLD+1", mTestEtldPlusOne, errorProperties.mRpForDisplay);
            assertEquals("Incorrect token error", mTokenError, errorProperties.mError);

            assertNotNull(
                    mModel.get(ItemProperties.CONTINUE_BUTTON)
                            .get(ContinueButtonProperties.PROPERTIES)
                            .mOnClickListener);
            assertNotNull(
                    mModel.get(ItemProperties.ERROR_TEXT)
                            .get(ErrorProperties.PROPERTIES)
                            .mMoreDetailsClickRunnable);

            // Do not let test inputs be ignored.
            mMediator.setComponentShowTime(-1000);
            mModel.get(ItemProperties.ERROR_TEXT)
                    .get(ErrorProperties.PROPERTIES)
                    .mMoreDetailsClickRunnable
                    .run();
            verify(mMockDelegate, times(++count)).onMoreDetails();
            verify(mMockDelegate, times(count))
                    .onDismissed(IdentityRequestDialogDismissReason.MORE_DETAILS_BUTTON);
            assertTrue(mMediator.wasDismissed());
            // Reset mediator after dismiss.
            resetMediator();
        }
    }

    /**
     * Tests that passive mode never populates the identity provider's links on the disclosure text,
     * even with `FedCmIdpLinks` enabled. This uses multiple identity providers without
     * `FedCmActiveModeMultipleIdentityProviders`, where the passive mode sheet still asks for
     * permission itself instead of switching to the active mode sheet.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    @DisableFeatures(ContentFeatures.FED_CM_ACTIVE_MODE_MULTIPLE_IDENTITY_PROVIDERS)
    public void testDataSharingConsentHasNoIdpLinks() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        // Build a genuinely distinct second IdP (`mIdpDataWithUseDifferentAccount` and
        // `mIdpDataWithoutIcons` share `mIdpData`'s domain and config URL).
        IdentityProviderData otherIdpData =
                new IdentityProviderData(
                        "other-idp.example",
                        new IdentityProviderMetadata(
                                Color.BLUE,
                                Color.GREEN,
                                /* brandIconBitmap= */ null,
                                new GURL("https://other-idp.example/fedcm.json"),
                                new GURL("https://other-idp.example/login"),
                                /* accountManagementUrl= */ GURL.emptyGURL(),
                                /* privacyPolicyUrl= */ GURL.emptyGURL(),
                                /* termsOfServiceUrl= */ GURL.emptyGURL(),
                                /* showUseDifferentAccountButton= */ false),
                        new ClientIdMetadata(
                                mTestUrlTermsOfService,
                                mTestUrlPrivacyPolicy,
                                /* brandIconBitmap= */ null),
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);
        Account otherIdpAccount =
                new Account(
                        "other",
                        "other@other-idp.example",
                        "Other Account",
                        "Other",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ true,
                        /* isBrowserTrustedSignIn= */ true,
                        /* isFilteredOut= */ false,
                        /* fields= */ new int[0],
                        otherIdpData);
        mMediator.showAccounts(
                createRpData(),
                Arrays.asList(mNewUserAccount, otherIdpAccount),
                Arrays.asList(mIdpData, otherIdpData),
                /* newAccounts= */ Collections.emptyList());
        clickAccount(mNewUserAccount);

        verify(mMockActiveSheetSwitcher, never())
                .switchToActiveSheetForPermission(any(), any(), any(), any());
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        DataSharingConsentProperties.Properties properties =
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES);
        assertTrue(properties.mIdpAccountManagementUrl.isEmpty());
        assertTrue(properties.mIdpPrivacyPolicyUrl.isEmpty());
        assertTrue(properties.mIdpTermsOfServiceUrl.isEmpty());
    }

    /**
     * Tests that with `FedCmIdpLinks`, the single-account chooser for a new account shows no
     * disclosure text, and that continuing there switches to the active mode sheet for permission
     * without notifying the delegate that the account was selected.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testContinueOnSingleNewAccountSwitchesToActiveSheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        RelyingPartyData rpData = createRpData();
        List<Account> accounts = Arrays.asList(mNewUserAccount);
        List<IdentityProviderData> idpDataList = Arrays.asList(mIdpData);
        mMediator.showAccounts(
                rpData, accounts, idpDataList, /* newAccounts= */ Collections.emptyList());

        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(
                mNewUserAccount,
                mModel.get(ItemProperties.CONTINUE_BUTTON)
                        .get(ContinueButtonProperties.PROPERTIES)
                        .mAccount);

        clickContinueButton();

        verify(mMockActiveSheetSwitcher)
                .switchToActiveSheetForPermission(rpData, accounts, idpDataList, mNewUserAccount);
        verify(mMockDelegate, never()).onAccountSelected(any());
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    /**
     * Tests that with `FedCmIdpLinks`, selecting a new account in the account chooser switches to
     * the active mode sheet for permission without notifying the delegate that the account was
     * selected.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testSelectingNewAccountSwitchesToActiveSheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        RelyingPartyData rpData = createRpData();
        List<Account> accounts = Arrays.asList(mAnaAccount, mNewUserAccount);
        List<IdentityProviderData> idpDataList = Arrays.asList(mIdpData);
        mMediator.showAccounts(
                rpData, accounts, idpDataList, /* newAccounts= */ Collections.emptyList());

        clickAccount(mNewUserAccount);

        verify(mMockActiveSheetSwitcher)
                .switchToActiveSheetForPermission(rpData, accounts, idpDataList, mNewUserAccount);
        verify(mMockDelegate, never()).onAccountSelected(any());
    }

    /**
     * Tests that with `FedCmIdpLinks` and `FedCmActiveModeMultipleIdentityProviders`, selecting a
     * new account in the account chooser for multiple identity providers switches to the active
     * mode sheet for permission without notifying the delegate that the account was selected.
     */
    @Test
    @EnableFeatures({
        ChromeFeatureList.FED_CM_IDP_LINKS,
        ContentFeatures.FED_CM_ACTIVE_MODE_MULTIPLE_IDENTITY_PROVIDERS
    })
    public void testSelectingNewAccountWithMultipleIdpsSwitchesToActiveSheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        RelyingPartyData rpData = createRpData();
        // `mIdpData2` offers to use a different account, which in passive mode means that it has a
        // filtered out account.
        List<Account> accounts =
                Arrays.asList(mAnaAccount, mNewUserAccountForIdp2, mFilteredOutAccountForIdp2);
        List<IdentityProviderData> idpDataList = Arrays.asList(mIdpData, mIdpData2);
        mMediator.showAccounts(
                rpData, accounts, idpDataList, /* newAccounts= */ Collections.emptyList());

        clickAccount(mNewUserAccountForIdp2);

        verify(mMockActiveSheetSwitcher)
                .switchToActiveSheetForPermission(
                        rpData, accounts, idpDataList, mNewUserAccountForIdp2);
        verify(mMockDelegate, never()).onAccountSelected(any());
    }

    /**
     * Tests that with `FedCmIdpLinks`, selecting a returning account still shows the verifying
     * sheet in the passive mode sheet.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testSelectingReturningAccountShowsVerifySheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                createRpData(),
                Arrays.asList(mAnaAccount, mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());

        clickAccount(mAnaAccount);

        verify(mMockDelegate).onAccountSelected(mAnaAccount);
        assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockActiveSheetSwitcher, never())
                .switchToActiveSheetForPermission(any(), any(), any(), any());
    }

    /**
     * Tests that without `FedCmIdpLinks`, the single-account chooser for a new account shows the
     * disclosure text, and continuing there selects the account in the passive mode sheet.
     */
    @Test
    public void testContinueOnSingleNewAccountWithoutIdpLinksSelectsAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                createRpData(),
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* newAccounts= */ Collections.emptyList());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        clickContinueButton();

        verify(mMockDelegate).onAccountSelected(mNewUserAccount);
        assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockActiveSheetSwitcher, never())
                .switchToActiveSheetForPermission(any(), any(), any(), any());
    }

    /**
     * Tests that with `FedCmActiveModeMultipleIdentityProviders` but without `FedCmIdpLinks`,
     * selecting a new account in the account chooser for multiple identity providers asks for
     * permission in the passive mode sheet, and continuing there selects the account.
     */
    @Test
    @EnableFeatures(ContentFeatures.FED_CM_ACTIVE_MODE_MULTIPLE_IDENTITY_PROVIDERS)
    public void testSelectingNewAccountWithMultipleIdpsWithoutIdpLinksSelectsAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                createRpData(),
                Arrays.asList(mAnaAccount, mNewUserAccountForIdp2, mFilteredOutAccountForIdp2),
                Arrays.asList(mIdpData, mIdpData2),
                /* newAccounts= */ Collections.emptyList());

        clickAccount(mNewUserAccountForIdp2);

        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        verify(mMockDelegate, never()).onAccountSelected(any());

        clickContinueButton();

        verify(mMockDelegate).onAccountSelected(mNewUserAccountForIdp2);
        assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
        verify(mMockActiveSheetSwitcher, never())
                .switchToActiveSheetForPermission(any(), any(), any(), any());
    }

    private RelyingPartyData createRpData() {
        return new RelyingPartyData(
                mTestEtldPlusOne, /* iframeForDisplay= */ "", /* rpIcon= */ null);
    }

    /** Clicks the row of {@code account} in the account chooser the same way the view does. */
    private void clickAccount(Account account) {
        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        for (MVCListAdapter.ListItem item : mSheetAccountItems) {
            if (item.type == AccountSelectionProperties.ITEM_TYPE_ACCOUNT
                    && item.model.get(AccountProperties.ACCOUNT) == account) {
                item.model
                        .get(AccountProperties.ON_CLICK_LISTENER)
                        .onResult(new ButtonData(account, /* idpMetadata= */ null));
                return;
            }
        }
        throw new AssertionError("No row for " + account.getId());
    }
}
