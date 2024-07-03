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
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IFRAME_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TOP_FRAME_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityCredentialTokenError;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.content.webid.IdentityRequestDialogDismissReason;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;

/**
 * Controller tests verify that the Account Selection delegate modifies the model if the API is used
 * properly. This class is parameterized to run all tests for each RP mode.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountSelectionControllerTest {
    @Parameter(0)
    public @RpMode.EnumType int mRpMode;

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {RpMode.WIDGET, RpMode.BUTTON});
    }

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private static final String TEST_ERROR_CODE = "invalid_request";
    private static final String[] RP_CONTEXTS =
            new String[] {"signin", "signup", "use", "continue"};
    private static final @Px int DESIRED_AVATAR_SIZE = 100;

    // Constants but can only be initialized after parameterized test runner setup.
    private String mTestEtldPlusOne;
    private String mTestEtldPlusOne1;
    private String mTestEtldPlusOne2;
    private GURL mTestProfilePic;
    private GURL mTestUrlTermsOfService;
    private GURL mTestUrlPrivacyPolicy;
    private GURL mTestIdpBrandIconUrl;
    private GURL mTestConfigUrl;
    private GURL mTestLoginUrl;
    private GURL mTestErrorUrl;
    private GURL mTestEmptyErrorUrl;

    private Account mAnaAccount;
    private Account mBobAccount;
    private Account mCarlAccount;
    private Account mNewUserAccount;
    private ClientIdMetadata mClientIdMetadata;
    private IdentityCredentialTokenError mTokenError;
    private IdentityCredentialTokenError mTokenErrorEmptyUrl;

    // Needs Bitmap.class Mockito mock for initialization.
    public IdentityProviderMetadata mIdpMetadata;

    @Mock private AccountSelectionComponent.Delegate mMockDelegate;
    @Mock private ImageFetcher mMockImageFetcher;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private Tab mTab;

    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private AccountSelectionMediator mMediator;
    private final PropertyModel mModel =
            new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS).build();
    private final ModelList mSheetAccountItems = new ModelList();

    public AccountSelectionControllerTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() {
        // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of
        // this test.
        mTestEtldPlusOne = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        mTestEtldPlusOne1 = JUnitTestGURLs.URL_1.getSpec();
        mTestEtldPlusOne2 = JUnitTestGURLs.URL_2.getSpec();
        mTestProfilePic = JUnitTestGURLs.URL_1_WITH_PATH;
        mTestUrlTermsOfService = JUnitTestGURLs.RED_1;
        mTestUrlPrivacyPolicy = JUnitTestGURLs.RED_2;
        mTestIdpBrandIconUrl = JUnitTestGURLs.RED_3;
        mTestConfigUrl = JUnitTestGURLs.URL_2;
        mTestLoginUrl = JUnitTestGURLs.URL_3;
        mTestErrorUrl = JUnitTestGURLs.URL_2;
        mTestEmptyErrorUrl = new GURL("");

        mAnaAccount =
                new Account(
                        "Ana",
                        "ana@one.test",
                        "Ana Doe",
                        "Ana",
                        mTestProfilePic,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
        mBobAccount =
                new Account(
                        "Bob",
                        "",
                        "Bob",
                        "",
                        mTestProfilePic,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
        mCarlAccount =
                new Account(
                        "Carl",
                        "carl@three.test",
                        "Carl Test",
                        ":)",
                        mTestProfilePic,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ true);
        mNewUserAccount =
                new Account(
                        "602214076",
                        "goto@email.example",
                        "Sam E. Goto",
                        "Sam",
                        mTestProfilePic,
                        /* pictureBitmap= */ null,
                        /* isSignIn= */ false);

        mClientIdMetadata = new ClientIdMetadata(mTestUrlTermsOfService, mTestUrlPrivacyPolicy);
        mTokenError = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestErrorUrl);
        mTokenErrorEmptyUrl = new IdentityCredentialTokenError(TEST_ERROR_CODE, mTestEmptyErrorUrl);

        mIdpMetadata =
                new IdentityProviderMetadata(
                        Color.BLACK,
                        Color.BLACK,
                        mTestIdpBrandIconUrl.getSpec(),
                        mTestConfigUrl,
                        mTestLoginUrl,
                        /* supports_add_account= */ false);

        mBottomSheetContent = new AccountSelectionBottomSheetContent(null, null);
        mMediator =
                new AccountSelectionMediator(
                        mTab,
                        mMockDelegate,
                        mModel,
                        mSheetAccountItems,
                        mMockBottomSheetController,
                        mBottomSheetContent,
                        mMockImageFetcher,
                        DESIRED_AVATAR_SIZE,
                        mRpMode);
    }

    public ArgumentMatcher<ImageFetcher.Params> imageFetcherParamsHaveUrl(GURL url) {
        return params -> params != null && params.url.equals(url.getSpec());
    }

    @Test
    public void testShowAccountSignInHeader() {
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(mTestEtldPlusOne, headerModel.get(TOP_FRAME_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne1, headerModel.get(IFRAME_FOR_DISPLAY));
        assertEquals(mTestEtldPlusOne2, headerModel.get(IDP_FOR_DISPLAY));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
    }

    @Test
    public void testBrandIconDownloadFails() {
        doAnswer(
                        new Answer<Void>() {
                            @Override
                            public Void answer(InvocationOnMock invocation) {
                                Callback<Bitmap> callback =
                                        (Callback<Bitmap>) invocation.getArguments()[1];
                                callback.onResult(null);
                                return null;
                            }
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any(Callback.class));

        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        // Brand icon should be transparent placeholder icon. This is useful so that the header text
        // wrapping does not change in the case that the brand icon download succeeds.
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
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
                        /* supports_add_account= */ false);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                idpMetadataNoBrandIconUrl,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(IDP_BRAND_ICON));

        // There should be no downloads.
        verify(mMockImageFetcher, times(0)).fetchImage(any(), any());
    }

    @Test
    public void testShowAccountSignUpHeader() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        assertEquals(3, countAllItems()); // Header + two Accounts
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Collections.singletonList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals(
                "Incorrect account", mAnaAccount, mSheetAccountItems.get(0).model.get(ACCOUNT));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Collections.singletonList(mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mCarlAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        verify(mMockBottomSheetController, times(1)).requestShowContent(any(), eq(true));

        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mCarlAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerSelectSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        mMediator.onAccountSelected(mNewUserAccount);

        assertFalse(mMediator.wasDismissed());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(1, mSheetAccountItems.size());

        verify(mMockDelegate, never()).onAccountSelected(mTestConfigUrl, mNewUserAccount);
    }

    @Test
    public void testShowsAccountPickerOnTosDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount, mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ true,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ true,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ false);
        // Because requestPermission is false, we expect header + account + continue btn
        assertEquals(3, countAllItems());
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
    }

    @Test
    public void testMultiAccountSkipConsentSheetWithoutRequestPermission() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                mTestEtldPlusOne,
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mNewUserAccount, mBobAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ false);
        mMediator.onAccountSelected(mNewUserAccount);
        verify(mMockDelegate).onAccountSelected(mTestConfigUrl, mNewUserAccount);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowVerifySheetExplicitSignin() {
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne1,
                    mTestEtldPlusOne2,
                    Arrays.asList(mNewUserAccount),
                    mIdpMetadata,
                    mClientIdMetadata,
                    /* isAutoReauthn= */ false,
                    rpContext,
                    /* requestPermission= */ true);
            mMediator.showVerifySheet(mAnaAccount);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
        }
    }

    @Test
    public void testShowVerifySheetAutoReauthn() {
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            // showVerifySheet is called in showAccounts when isAutoReauthn is true
            mMediator.showAccounts(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne1,
                    mTestEtldPlusOne2,
                    Arrays.asList(mAnaAccount),
                    mIdpMetadata,
                    mClientIdMetadata,
                    /* isAutoReauthn= */ true,
                    rpContext,
                    /* requestPermission= */ true);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
            verify(mMockDelegate).onAccountsDisplayed();
        }
    }

    @Test
    public void testShowFailureDialog() {
        int count = 0;
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showFailureDialog(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne1,
                    mTestEtldPlusOne2,
                    mIdpMetadata,
                    rpContext);
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
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne1,
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
            assertEquals(
                    "Incorrect top frame ETLD+1",
                    mTestEtldPlusOne,
                    errorProperties.mTopFrameForDisplay);
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
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    mTestEtldPlusOne,
                    mTestEtldPlusOne1,
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
            assertEquals(
                    "Incorrect top frame ETLD+1",
                    mTestEtldPlusOne,
                    errorProperties.mTopFrameForDisplay);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
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
                mTestEtldPlusOne1,
                mTestEtldPlusOne2,
                Arrays.asList(mAnaAccount),
                mIdpMetadata,
                mClientIdMetadata,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin",
                /* requestPermission= */ true);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        mMediator.getTabObserver().onInteractabilityChanged(mTab, true);
        verify(mMockBottomSheetController, times(1)).requestShowContent(mBottomSheetContent, true);
    }

    private void pressBack() {
        if (mBottomSheetContent.handleBackPress()) return;

        mMediator.onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    private int countAllItems() {
        int count = 0;
        for (PropertyKey key : mModel.getAllProperties()) {
            if (containsItemOfType(mModel, key)) {
                count += 1;
            }
        }
        return count + mSheetAccountItems.size();
    }

    private static boolean containsItemOfType(PropertyModel model, PropertyKey key) {
        return model.get((WritableObjectPropertyKey<PropertyModel>) key) != null;
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
