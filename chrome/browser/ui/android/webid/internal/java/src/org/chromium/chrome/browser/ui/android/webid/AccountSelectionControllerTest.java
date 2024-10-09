// Copyright 2021 The Chromium Authors
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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ACCOUNT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_MODE;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SET_FOCUS_VIEW_CALLBACK;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;

/**
 * Controller tests verify that the Account Selection delegate modifies the model. This class is
 * parameterized to run all tests for each RP mode.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountSelectionControllerTest extends AccountSelectionJUnitTestBase {
    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {RpMode.PASSIVE, RpMode.ACTIVE});
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    public ArgumentMatcher<ImageFetcher.Params> imageFetcherParamsHaveUrl(GURL url) {
        return params -> params != null && params.url.equals(url.getSpec());
    }

    @Test
    public void testSingleAccountSignInHeader() {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback<Bitmap> callback =
                                        (Callback<Bitmap>) invocation.getArguments()[1];

                                Bitmap brandIcon =
                                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                                brandIcon.eraseColor(Color.RED);
                                callback.onResult(brandIcon);
                                return null;
                            }
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any(Callback.class));

        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne, headerModel.get(RP_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
        assertEquals((Integer) mRpMode, headerModel.get(RP_MODE));
        assertFalse(headerModel.get(IS_MULTIPLE_ACCOUNT_CHOOSER));
    }

    @Test
    public void testMultipleAccountsSignInHeader() {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback<Bitmap> callback =
                                        (Callback<Bitmap>) invocation.getArguments()[1];

                                Bitmap brandIcon =
                                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
                                brandIcon.eraseColor(Color.RED);
                                callback.onResult(brandIcon);
                                return null;
                            }
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any(Callback.class));

        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne, headerModel.get(RP_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
        assertEquals((Integer) mRpMode, headerModel.get(RP_MODE));
        assertTrue(headerModel.get(IS_MULTIPLE_ACCOUNT_CHOOSER));
    }

    /**
     * Test that the FedCM account picker does not display the brand icon placeholder if the brand
     * icon URL is empty.
     */
    @Test
    public void testNoBrandIconUrl() {
        IdentityProviderMetadata idpMetadataNoBrandIconUrl =
                new IdentityProviderMetadata(
                        Color.BLACK,
                        Color.BLACK,
                        "",
                        mTestConfigUrl,
                        mTestLoginUrl,
                        /* supportsAddAccount= */ false);
        ClientIdMetadata clientMetadataNoBrandIconUrl =
                new ClientIdMetadata(
                        mTestUrlTermsOfService, mTestUrlPrivacyPolicy, /* brandIconUrl= */ "");

        IdentityProviderData idpData =
                new IdentityProviderData(
                        mTestEtldPlusOne2,
                        idpMetadataNoBrandIconUrl,
                        clientMetadataNoBrandIconUrl,
                        RpContext.SIGN_IN,
                        DEFAULT_DISCLOSURE_FIELDS,
                        /* isAutoReauthn= */ false);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                idpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(IDP_BRAND_ICON));

        // There should be no downloads.
        verify(mMockImageFetcher, times(0)).fetchImage(any(), any());
    }

    @Test
    public void testShowAccountSignUpHeader() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(3, countAllItems()); // Header + two Accounts
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Collections.singletonList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals(
                "Incorrect account", mAnaAccount, mSheetAccountItems.get(0).model.get(ACCOUNT));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Collections.singletonList(mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals(
                "Incorrect account", mBobAccount, mSheetAccountItems.get(0).model.get(ACCOUNT));
    }

    @Test
    public void testShowAccountsSetsVisible() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mCarlAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        verify(mMockBottomSheetController, times(1)).requestShowContent(any(), eq(true));

        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        assertFalse(mMediator.wasDismissed());
        assertNotNull(
                mModel.get(ItemProperties.CONTINUE_BUTTON)
                        .get(ContinueButtonProperties.PROPERTIES)
                        .mOnClickListener);

        mModel.get(ItemProperties.CONTINUE_BUTTON)
                .get(ContinueButtonProperties.PROPERTIES)
                .mOnClickListener
                .onResult(mAnaAccount);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mCarlAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        assertFalse(mMediator.wasDismissed());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));

        mSheetAccountItems
                .get(0)
                .model
                .get(AccountProperties.ON_CLICK_LISTENER)
                .onResult(mCarlAccount);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mCarlAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnSingleAccountDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerSelectSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(mAnaAccount);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowsTosOnMultiAccountSelectSignUp() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(mNewUserAccount);

        assertFalse(mMediator.wasDismissed());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        verify(mMockDelegate, never()).onAccountSelected(mTestConfigUrl, mNewUserAccount);
    }

    @Test
    public void testShowsAccountPickerOnTosDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(mNewUserAccount);

        pressBack();
        assertFalse(mMediator.wasDismissed());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(2, mSheetAccountItems.size());

        pressBack();
        assertTrue(mMediator.wasDismissed());

        verify(mMockDelegate, never()).onAccountSelected(mTestConfigUrl, mNewUserAccount);
    }

    @Test
    public void testNotShowAccountPickerOnVerifyingUiDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(2, mSheetAccountItems.size());
        mMediator.onAccountSelected(mAnaAccount);

        pressBack();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ true,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Auto reauthenticates if no action is taken.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnlyOnceWithAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ true,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Auto reauthenticates even if dismissed.
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mAnaAccount);
        verify(mMockDelegate).onAccountsDisplayed();
        verifyNoMoreInteractions(mMockDelegate);
        assertTrue(mMediator.wasDismissed());
        // The delayed task should not call delegate after user dismissing.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testShowDataSharingConsentForSingleNewAccount() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // For new user we expect header + account + consent text + continue btn
        assertEquals(4, countAllItems());
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        DataSharingConsentProperties.Properties dataSharingProperties =
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES);
        assertEquals(
                "Incorrect privacy policy URL",
                mTestUrlPrivacyPolicy,
                dataSharingProperties.mPrivacyPolicyUrl);
        assertEquals(
                "Incorrect terms of service URL",
                mTestUrlTermsOfService,
                dataSharingProperties.mTermsOfServiceUrl);
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        assertEquals(
                "Incorrect provider ETLD+1",
                mTestEtldPlusOne2,
                dataSharingProperties.mIdpForDisplay);
    }

    @Test
    public void testNewUserWithoutRequestPermission() {
        mIdpData.setDisclosureFields(new int[0]);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Because disclosureFields are empty, we expect header + account + continue btn
        assertEquals(3, countAllItems());
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
    }

    @Test
    public void testMultiAccountSkipConsentSheetWithoutRequestPermission() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mIdpData.setDisclosureFields(new int[0]);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount, mBobAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(mNewUserAccount);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mNewUserAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowFailureDialog() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showFailureDialog(
                    mTestEtldPlusOne, mTestEtldPlusOne2, mIdpMetadata, rpContext);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.SIGN_IN_TO_IDP_STATIC, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate, never()).onAccountsDisplayed();
            // For failure dialog, we expect header + IDP sign in text + continue btn
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.IDP_SIGNIN));

            String idpEtldPlusOne =
                    mModel.get(ItemProperties.IDP_SIGNIN).get(IdpSignInProperties.IDP_FOR_DISPLAY);
            assertEquals("Incorrect provider ETLD+1", mTestEtldPlusOne2, idpEtldPlusOne);

            assertNotNull(
                    mModel.get(ItemProperties.CONTINUE_BUTTON)
                            .get(ContinueButtonProperties.PROPERTIES)
                            .mOnClickListener);

            // Do not let test inputs be ignored.
            mMediator.setComponentShowTime(-1000);
            mModel.get(ItemProperties.CONTINUE_BUTTON)
                    .get(ContinueButtonProperties.PROPERTIES)
                    .mOnClickListener
                    .onResult(null);
            verify(mMockDelegate, times(++count)).onLoginToIdP(mTestConfigUrl, mTestLoginUrl);
        }
    }

    @Test
    public void testShowErrorDialogClickGotIt() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne2,
                    mIdpMetadata,
                    rpContext,
                    mTokenErrorEmptyUrl);
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
                    .onResult(mAnaAccount);
            verify(mMockDelegate, times(++count))
                    .onDismissed(IdentityRequestDialogDismissReason.GOT_IT_BUTTON);
            assertTrue(mMediator.wasDismissed());
        }
    }

    @Test
    public void testShowErrorDialogClickMoreDetails() {
        int count = 0;
        for (int rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne, mTestEtldPlusOne2, mIdpMetadata, rpContext, mTokenError);
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
        }
    }

    @Test
    public void testKeyboardShowingAndHiding() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        KeyboardVisibilityListener listener = mMediator.getKeyboardEventListener();
        listener.keyboardVisibilityChanged(true);
        verify(mMockBottomSheetController).hideContent(mBottomSheetContent, true);
        when(mTab.isUserInteractable()).thenReturn(true);
        listener.keyboardVisibilityChanged(false);
        verify(mMockBottomSheetController, times(2)).requestShowContent(mBottomSheetContent, true);
        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testWebContentsInteractibilityChange() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.getTabObserver().onInteractabilityChanged(mTab, false);
        verify(mMockBottomSheetController).hideContent(mBottomSheetContent, false);
        mMediator.getTabObserver().onInteractabilityChanged(mTab, true);
        verify(mMockBottomSheetController, times(2)).requestShowContent(mBottomSheetContent, true);
        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testNavigationInPrimaryMainFrame() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // We pass null as |mMediatior| does not really care about where we navigate to.
        mMediator.getTabObserver().onDidStartNavigationInPrimaryMainFrame(mTab, null);
        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    @Test
    public void testShowKeyboardWhileNotInteractable() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        KeyboardVisibilityListener listener = mMediator.getKeyboardEventListener();
        listener.keyboardVisibilityChanged(true);
        verify(mMockBottomSheetController).hideContent(mBottomSheetContent, true);

        when(mTab.isUserInteractable()).thenReturn(false);

        // Showing the keyboard again should do nothing since the tab is not interactable!
        listener.keyboardVisibilityChanged(false);
        // The requestShowContent method should have been called only once.
        verify(mMockBottomSheetController, times(1)).requestShowContent(mBottomSheetContent, true);
        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testWebContentsHidden() {
        when(mTab.isHidden()).thenReturn(true);
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        mMediator.getTabObserver().onInteractabilityChanged(mTab, true);
        verify(mMockBottomSheetController, times(1)).requestShowContent(mBottomSheetContent, true);
    }

    @Test
    public void testSetFocusViewCallback() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpData,
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        assertNotNull(mModel.get(ItemProperties.HEADER).get(SET_FOCUS_VIEW_CALLBACK));
        assertNotNull(
                mModel.get(ItemProperties.CONTINUE_BUTTON)
                        .get(ContinueButtonProperties.PROPERTIES)
                        .mSetFocusViewCallback);
        assertNotNull(
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES)
                        .mSetFocusViewCallback);
    }

    @Test
    public void testnewAccountsMultipleAccountsShowsAccountChooser() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne2,
                mNewAccountsMultipleAccounts,
                mIdpData,
                /* isAutoReauthn= */ false,
                mNewAccountsMultipleAccounts);

        // Account chooser is shown for multiple newly signed-in accounts.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    private void pressBack() {
        if (mBottomSheetContent.handleBackPress()) return;

        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    private static int countListItemsOfType(ModelList list, int searchType) {
        int count = 0;
        for (ListItem item : list) {
            if (item.type == searchType) {
                count += 1;
            }
        }
        return count;
    }
}
