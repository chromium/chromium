// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.RelyingPartyData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Tests that AccountSelectionCoordinator replaces the passive mode sheet with the active mode one
 * when a passive mode request asks for permission through the active mode sheet, and which sheet
 * shows the dialogs that can follow.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
public class AccountSelectionCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String IDP_FOR_DISPLAY = "idp.example";

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AccountSelectionComponent.Delegate mMockDelegate;

    private static class TestCustomTabActivity extends CustomTabActivity {
        private BrowserServicesIntentDataProvider mIntentDataProvider;

        void setIntentDataProviderForTesting(BrowserServicesIntentDataProvider provider) {
            mIntentDataProvider = provider;
        }

        @Override
        public BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mIntentDataProvider;
        }
    }

    private final RelyingPartyData mRpData =
            new RelyingPartyData("rp.example", /* iframeForDisplay= */ "", /* rpIcon= */ null);
    private Activity mActivity;
    private IdentityProviderMetadata mIdpMetadata;
    private IdentityProviderData mIdpData;
    private Account mNewAccount;
    private AccountSelectionCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mActivity));
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<Activity>(mActivity));
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);

        mIdpMetadata =
                new IdentityProviderMetadata(
                        Color.BLUE,
                        Color.GREEN,
                        /* brandIconBitmap= */ null,
                        new GURL("https://idp.example/fedcm.json"),
                        new GURL("https://idp.example/login"),
                        new GURL("https://idp.example/account"),
                        new GURL("https://idp.example/privacy"),
                        new GURL("https://idp.example/terms"),
                        /* showUseDifferentAccountButton= */ false);
        mIdpData =
                new IdentityProviderData(
                        IDP_FOR_DISPLAY,
                        mIdpMetadata,
                        new ClientIdMetadata(
                                new GURL("https://rp.example/terms"),
                                new GURL("https://rp.example/privacy"),
                                /* brandIconBitmap= */ null),
                        RpContext.SIGN_IN,
                        AccountSelectionJUnitTestBase.DEFAULT_DISCLOSURE_FIELDS,
                        /* hasLoginStatusMismatch= */ false);
        mNewAccount =
                new Account(
                        "new",
                        "new@idp.example",
                        "New User",
                        "New",
                        /* secondaryDescription= */ null,
                        /* pictureBitmap= */ null,
                        /* circledBadgedPictureBitmap= */ null,
                        /* isIdpClaimedSignIn= */ false,
                        /* isBrowserTrustedSignIn= */ false,
                        /* isFilteredOut= */ false,
                        AccountSelectionJUnitTestBase.DEFAULT_DISCLOSURE_FIELDS,
                        mIdpData);

        mCoordinator =
                new AccountSelectionCoordinator(
                        mTab,
                        mWindowAndroid,
                        mBottomSheetController,
                        RpMode.PASSIVE,
                        /* canShowUi= */ true,
                        mMockDelegate);
    }

    /**
     * Tests that continuing with a new account in the passive mode sheet hides it right away and
     * shows the active mode sheet on the request permission dialog, without notifying the delegate.
     */
    @Test
    public void testContinueWithNewAccountSwitchesToActiveSheet() {
        AccountSelectionMediator passiveMediator = mCoordinator.getMediator();
        switchToActiveSheet();

        AccountSelectionMediator activeMediator = mCoordinator.getMediator();
        assertNotSame(passiveMediator, activeMediator);
        assertTrue(passiveMediator.wasDismissed());
        assertFalse(activeMediator.wasDismissed());
        assertEquals(HeaderType.REQUEST_PERMISSION_MODAL, activeMediator.getHeaderType());

        List<BottomSheetContent> sheets = captureShownSheets(2);
        assertFalse(sheets.get(0).getSheetType().isModal());
        assertTrue(sheets.get(1).getSheetType().isModal());
        verify(mBottomSheetController).hideContent(sheets.get(0), /* animate= */ false);
        verify(mMockDelegate, never()).onAccountSelected(any());
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    /**
     * Tests that the failure dialog, which is only shown in passive mode, replaces the active mode
     * sheet with a new passive mode sheet.
     */
    @Test
    public void testFailureDialogAfterSwitchShowsPassiveSheet() {
        switchToActiveSheet();
        AccountSelectionMediator activeMediator = mCoordinator.getMediator();

        assertTrue(
                mCoordinator.showFailureDialog(
                        mRpData, IDP_FOR_DISPLAY, mIdpMetadata, RpContext.SIGN_IN));

        AccountSelectionMediator passiveMediator = mCoordinator.getMediator();
        assertNotSame(activeMediator, passiveMediator);
        assertTrue(activeMediator.wasDismissed());
        assertEquals(HeaderType.SIGN_IN_TO_IDP_STATIC, passiveMediator.getHeaderType());
        List<BottomSheetContent> sheets = captureShownSheets(3);
        assertFalse(sheets.get(2).getSheetType().isModal());
        verify(mBottomSheetController).hideContent(sheets.get(1), /* animate= */ false);
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    /**
     * Tests that a sheet that replaces another stays hidden while FedCM UI can't be shown, and is
     * shown once it can.
     */
    @Test
    public void testReplacingSheetKeepsCanShowUi() {
        switchToActiveSheet();
        mCoordinator.setCanShowUi(false);

        assertTrue(
                mCoordinator.showFailureDialog(
                        mRpData, IDP_FOR_DISPLAY, mIdpMetadata, RpContext.SIGN_IN));
        // While FedCM UI can't be shown, the new passive mode sheet isn't shown.
        captureShownSheets(2);

        mCoordinator.setCanShowUi(true);
        assertFalse(captureShownSheets(3).get(2).getSheetType().isModal());
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    /** Tests that the error dialog stays in active mode once the active mode sheet is shown. */
    @Test
    public void testErrorDialogAfterSwitchStaysInActiveMode() {
        switchToActiveSheet();
        AccountSelectionMediator activeMediator = mCoordinator.getMediator();

        assertTrue(
                mCoordinator.showErrorDialog(
                        mRpData,
                        IDP_FOR_DISPLAY,
                        mIdpMetadata,
                        RpContext.SIGN_IN,
                        new IdentityCredentialTokenError(
                                AccountSelectionJUnitTestBase.TEST_ERROR_CODE, GURL.emptyGURL())));

        assertSame(activeMediator, mCoordinator.getMediator());
        assertEquals(HeaderType.SIGN_IN_ERROR, activeMediator.getHeaderType());
        // Active mode shows the error in a modal dialog.
        verify(mModalDialogManager).showDialog(any(), eq(ModalDialogManager.ModalDialogType.APP));
        verify(mMockDelegate, never()).onDismissed(anyInt());
    }

    /** Tests that closing the FedCM UI after the switch hides the active mode sheet. */
    @Test
    public void testCloseAfterSwitchHidesActiveSheet() {
        switchToActiveSheet();
        AccountSelectionMediator activeMediator = mCoordinator.getMediator();
        BottomSheetContent activeSheet = captureShownSheets(2).get(1);

        mCoordinator.close();

        assertTrue(activeMediator.wasDismissed());
        verify(mBottomSheetController).hideContent(activeSheet, /* animate= */ true);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE)
    @DisableFeatures(ContentFeatures.FED_CM_NATIVE_ID_PS)
    public void testCctModalDismissalReopensAccountChooser() {
        assertTrue(
                mCoordinator.showAccounts(
                        mRpData,
                        Arrays.asList(mNewAccount),
                        Arrays.asList(mIdpData),
                        /* newAccounts= */ Collections.emptyList()));
        mCoordinator.showModalDialog(new GURL("https://idp.example/login"));
        clearInvocations(mBottomSheetController);

        mCoordinator.onActivityResumed();

        verify(mMockDelegate, never()).onDismissed(anyInt());
        assertEquals(HeaderType.SIGN_IN, mCoordinator.getMediator().getHeaderType());
        verify(mBottomSheetController).requestShowContent(any(), eq(true));
    }

    @Test
    @DisableFeatures({
        ContentFeatures.FED_CM_NATIVE_ID_PS,
        ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE
    })
    public void testPopupCctAssociatesWithOpenerViaIntentDataProviderWithoutIntentExtra() {
        WebContents rpWebContents = mock(WebContents.class);
        when(mMockDelegate.getWebContents()).thenReturn(rpWebContents);

        mCoordinator.showModalDialog(new GURL("https://idp.example/login"));
        Intent launchedIntent = shadowOf(mActivity).getNextStartedActivity();
        int fedCmId = launchedIntent.getIntExtra(IntentHandler.EXTRA_FEDCM_ID, -1);
        assertTrue(fedCmId != -1);

        TestCustomTabActivity popupCctActivity =
                Robolectric.buildActivity(TestCustomTabActivity.class, new Intent()).get();
        BrowserServicesIntentDataProvider intentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        when(intentDataProvider.getFedCmId()).thenReturn(fedCmId);
        popupCctActivity.setIntentDataProviderForTesting(intentDataProvider);

        WindowAndroid popupWindowAndroid = mock(WindowAndroid.class);
        when(popupWindowAndroid.getContext()).thenReturn(new WeakReference<Context>(mActivity));
        when(popupWindowAndroid.getActivity())
                .thenReturn(new WeakReference<Activity>(popupCctActivity));
        when(popupWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);

        AccountSelectionCoordinator popupCoordinator =
                new AccountSelectionCoordinator(
                        mTab,
                        popupWindowAndroid,
                        mBottomSheetController,
                        RpMode.ACTIVE,
                        /* canShowUi= */ true,
                        mock(AccountSelectionComponent.Delegate.class));

        verify(mMockDelegate).setPopupComponent(popupCoordinator);
        assertSame(rpWebContents, popupCoordinator.getRpWebContents());
    }

    /** Shows the passive mode sheet for a new account and continues with it. */
    private void switchToActiveSheet() {
        assertTrue(
                mCoordinator.showAccounts(
                        mRpData,
                        Arrays.asList(mNewAccount),
                        Arrays.asList(mIdpData),
                        /* newAccounts= */ Collections.emptyList()));
        // Do not let test inputs be ignored.
        mCoordinator.getMediator().setComponentShowTime(-1000);
        captureShownSheets(1)
                .get(0)
                .getContentView()
                .findViewById(R.id.account_selection_continue_btn)
                .performClick();
    }

    /**
     * Returns the sheets that the coordinator asked the bottom sheet to show, in order. Only the
     * active mode sheet is modal.
     */
    private List<BottomSheetContent> captureShownSheets(int count) {
        ArgumentCaptor<BottomSheetContent> sheets =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        verify(mBottomSheetController, times(count))
                .requestShowContent(sheets.capture(), anyBoolean());
        return sheets.getAllValues();
    }
}
