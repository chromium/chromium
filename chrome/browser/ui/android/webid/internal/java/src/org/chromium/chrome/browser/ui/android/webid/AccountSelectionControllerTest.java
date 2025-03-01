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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ACCOUNT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IS_MULTIPLE_IDPS;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.RP_MODE;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SET_FOCUS_VIEW_CALLBACK;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.view.View;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AddAccountButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
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
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne, headerModel.get(RP_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
        assertEquals((Integer) mRpMode, headerModel.get(RP_MODE));
        assertFalse(headerModel.get(IS_MULTIPLE_ACCOUNT_CHOOSER));
        assertFalse(headerModel.get(IS_MULTIPLE_IDPS));
    }

    @Test
    public void testMultipleAccountsSignInHeader() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne, headerModel.get(RP_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
        assertEquals((Integer) mRpMode, headerModel.get(RP_MODE));
        assertTrue(headerModel.get(IS_MULTIPLE_ACCOUNT_CHOOSER));
        assertFalse(headerModel.get(IS_MULTIPLE_IDPS));
    }

    /**
     * Test that the FedCM account picker does not display the brand icon placeholder if the brand
     * icon is null.
     */
    @Test
    public void testNoBrandIcons() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpDataWithoutIcons),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(IDP_BRAND_ICON));
    }

    @Test
    public void testShowAccountSignUpHeader() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(3, countAllItems()); // Header + two Accounts
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Collections.singletonList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals(
                "Incorrect account", mAnaAccount, mSheetAccountItems.get(0).model.get(ACCOUNT));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Collections.singletonList(mBobAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount, mCarlAccount, mBobAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                .onResult(new ButtonData(mAnaAccount, /* idpMetadata= */ null));
        verify(mMockDelegate).onAccountSelected(mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mCarlAccount),
                Arrays.asList(mIdpData),
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
                .onResult(new ButtonData(mCarlAccount, /* idpMetadata= */ null));
        verify(mMockDelegate).onAccountSelected(mCarlAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnSingleAccountDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(new ButtonData(mAnaAccount, /* idpMetadata= */ null));
        verify(mMockDelegate).onAccountSelected(mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowsTosOnMultiAccountSelectSignUp() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(new ButtonData(mNewUserAccount, /* idpMetadata= */ null));

        assertFalse(mMediator.wasDismissed());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        verify(mMockDelegate, never()).onAccountSelected(mNewUserAccount);
    }

    @Test
    public void testShowsAccountPickerOnTosDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(new ButtonData(mNewUserAccount, /* idpMetadata= */ null));

        pressBack();
        assertFalse(mMediator.wasDismissed());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(2, mSheetAccountItems.size());

        pressBack();
        assertTrue(mMediator.wasDismissed());

        verify(mMockDelegate, never()).onAccountSelected(mNewUserAccount);
    }

    @Test
    public void testNotShowAccountPickerOnVerifyingUiDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        assertEquals(2, mSheetAccountItems.size());
        mMediator.onAccountSelected(new ButtonData(mAnaAccount, /* idpMetadata= */ null));

        pressBack();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ true,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Auto reauthenticates if no action is taken.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockDelegate).onAccountSelected(mAnaAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnlyOnceWithAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ true,
                /* newAccounts= */ Collections.EMPTY_LIST);
        // Auto reauthenticates even if dismissed.
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate).onAccountSelected(mAnaAccount);
        if (mRpMode == RpMode.PASSIVE) {
            verify(mMockDelegate).onAccountsDisplayed();
        }
        verifyNoMoreInteractions(mMockDelegate);
        assertTrue(mMediator.wasDismissed());
        // The delayed task should not call delegate after user dismissing.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testShowDataSharingConsentForSingleNewAccount() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mNewUserAccount, mBobAccount),
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);
        mMediator.onAccountSelected(new ButtonData(mNewUserAccount, /* idpMetadata= */ null));
        verify(mMockDelegate).onAccountSelected(mNewUserAccount);
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
                    .onResult(new ButtonData(/* account= */ null, mIdpMetadata));
            verify(mMockDelegate, times(++count)).onLoginToIdP(mTestConfigUrl, mTestLoginUrl);
        }
    }

    @Test
    public void testKeyboardShowingAndHiding() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mAnaAccount),
                Arrays.asList(mIdpData),
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
                Arrays.asList(mNewUserAccount),
                Arrays.asList(mIdpData),
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
                mNewAccountsMultipleAccounts,
                Arrays.asList(mIdpData),
                /* isAutoReauthn= */ false,
                mNewAccountsMultipleAccounts);

        // Account chooser is shown for multiple newly signed-in accounts.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    @Test
    public void testFilteredOutAccountNoClickListener() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(
                        mAnaAccountWithUseDifferentAccount,
                        mFilteredOutAccountWithUseDifferentAccount),
                Arrays.asList(mIdpDataWithUseDifferentAccount),
                /* isAutoReauthn= */ false,
                /* newAccounts= */ Collections.EMPTY_LIST);

        // Account chooser is shown.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));

        assertEquals(3, mSheetAccountItems.size());
        // First account has a click listener.
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));
        // Second account is filtered out, so does not.
        assertNull(mSheetAccountItems.get(1).model.get(AccountProperties.ON_CLICK_LISTENER));
        // Third is the use other account button.
        assertNotNull(mSheetAccountItems.get(2).model.get(AddAccountButtonProperties.PROPERTIES));

        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        assertEquals(3, sheetItemListView.getAdapter().getItemCount());

        View anaRow = sheetItemListView.getChildAt(0);
        assertEquals(anaRow.getAlpha(), 1.f, ALPHA_COMPARISON_DELTA);
        TextView textView = anaRow.findViewById(R.id.title);
        assertEquals("Ana Doe", textView.getText());
        textView = anaRow.findViewById(R.id.description);
        assertEquals("ana@email.example", textView.getText());

        View nicolasRow = sheetItemListView.getChildAt(1);
        assertEquals(
                nicolasRow.getAlpha(),
                AccountSelectionViewBinder.DISABLED_OPACITY,
                ALPHA_COMPARISON_DELTA);
        textView = nicolasRow.findViewById(R.id.title);
        assertEquals("nicolas@example.com", textView.getText());
        textView = nicolasRow.findViewById(R.id.description);
        assertEquals("You can’t sign in using this account", textView.getText());

        View addAccountButton = sheetItemListView.getChildAt(2);
        assertEquals(addAccountButton.getAlpha(), 1.f, ALPHA_COMPARISON_DELTA);
        textView = addAccountButton.findViewById(R.id.title);
        assertEquals("Use a different account", textView.getText());
    }

    @Test
    public void testFilteredOutAccountNoContinueButton() {
        // Show a newly logged in filtered account.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                Arrays.asList(mFilteredOutAccountWithUseDifferentAccount),
                Arrays.asList(mIdpDataWithUseDifferentAccount),
                /* isAutoReauthn= */ false,
                Arrays.asList(mFilteredOutAccountWithUseDifferentAccount));
        // Account chooser is shown.
        assertEquals(HeaderType.SIGN_IN, mModel.get(ItemProperties.HEADER).get(TYPE));
        assertEquals(2, mSheetAccountItems.size());
        assertNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));

        View sheetContainer = mContentView.findViewById(R.id.sheet_item_list_container);
        RecyclerView sheetItemListView = sheetContainer.findViewById(R.id.sheet_item_list);
        View filteredAccountRow = sheetItemListView.getChildAt(0);
        assertEquals(
                filteredAccountRow.getAlpha(),
                AccountSelectionViewBinder.DISABLED_OPACITY,
                ALPHA_COMPARISON_DELTA);
        TextView textView = filteredAccountRow.findViewById(R.id.title);
        assertEquals("nicolas@example.com", textView.getText());
        textView = filteredAccountRow.findViewById(R.id.description);
        assertEquals("You can’t sign in using this account", textView.getText());

        View addAccountButton = sheetItemListView.getChildAt(1);
        assertEquals(addAccountButton.getAlpha(), 1.f, ALPHA_COMPARISON_DELTA);
        textView = addAccountButton.findViewById(R.id.title);
        assertEquals("Use a different account", textView.getText());
        assertFalse(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
    }

    private void pressBack() {
        if (mBottomSheetContent.handleBackPress()) return;

        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }
}
