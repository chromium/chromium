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
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ACCOUNT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.AVATAR;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IFRAME_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TOP_FRAME_FOR_DISPLAY;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.Avatar;
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
import java.util.Collections;

/**
 * Controller tests verify that the Account Selection delegate modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountSelectionControllerTest {
    // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of this
    // test.
    private static final String TEST_ETLD_PLUS_ONE = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final String TEST_ETLD_PLUS_ONE_1 = JUnitTestGURLs.URL_1.getSpec();
    private static final String TEST_ETLD_PLUS_ONE_2 = JUnitTestGURLs.URL_2.getSpec();
    private static final String TEST_ERROR_CODE = "invalid_request";
    private static final GURL TEST_PROFILE_PIC = JUnitTestGURLs.URL_1_WITH_PATH;
    private static final GURL TEST_URL_TERMS_OF_SERVICE = JUnitTestGURLs.RED_1;
    private static final GURL TEST_URL_PRIVACY_POLICY = JUnitTestGURLs.RED_2;
    private static final GURL TEST_IDP_BRAND_ICON_URL = JUnitTestGURLs.RED_3;
    private static final GURL TEST_CONFIG_URL = JUnitTestGURLs.URL_2;
    private static final GURL TEST_LOGIN_URL = JUnitTestGURLs.URL_3;
    private static final GURL TEST_ERROR_URL = JUnitTestGURLs.URL_2;
    private static final GURL TEST_EMPTY_ERROR_URL = new GURL("");

    private static final Account ANA =
            new Account(
                    "Ana",
                    "ana@one.test",
                    "Ana Doe",
                    "Ana",
                    TEST_PROFILE_PIC,
                    /* isSignIn= */ true);
    private static final Account BOB =
            new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, /* isSignIn= */ true);
    private static final Account CARL =
            new Account(
                    "Carl",
                    "carl@three.test",
                    "Carl Test",
                    ":)",
                    TEST_PROFILE_PIC,
                    /* isSignIn= */ true);
    private static final Account NEW_USER =
            new Account(
                    "602214076",
                    "goto@email.example",
                    "Sam E. Goto",
                    "Sam",
                    TEST_PROFILE_PIC,
                    /* isSignIn= */ false);
    private static final String[] RP_CONTEXTS =
            new String[] {"signin", "signup", "use", "continue"};
    private static final ClientIdMetadata CLIENT_ID_METADATA =
            new ClientIdMetadata(TEST_URL_TERMS_OF_SERVICE, TEST_URL_PRIVACY_POLICY);
    private static final IdentityCredentialTokenError TOKEN_ERROR =
            new IdentityCredentialTokenError(TEST_ERROR_CODE, TEST_ERROR_URL);
    private static final IdentityCredentialTokenError TOKEN_ERROR_EMPTY_URL =
            new IdentityCredentialTokenError(TEST_ERROR_CODE, TEST_EMPTY_ERROR_URL);

    private static final @Px int DESIRED_AVATAR_SIZE = 100;

    // Needs Bitmap.class Mockito mock for initialization. Initialized in
    // AccountSelectionControllerTest constructor.
    public final IdentityProviderMetadata IDP_METADATA;

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
        IDP_METADATA =
                new IdentityProviderMetadata(
                        Color.BLACK,
                        Color.BLACK,
                        TEST_IDP_BRAND_ICON_URL.getSpec(),
                        TEST_CONFIG_URL,
                        TEST_LOGIN_URL);
    }

    @Before
    public void setUp() {
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
                        DESIRED_AVATAR_SIZE);
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
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(TEST_ETLD_PLUS_ONE, headerModel.get(TOP_FRAME_FOR_DISPLAY));
        assertEquals(TEST_ETLD_PLUS_ONE_1, headerModel.get(IFRAME_FOR_DISPLAY));
        assertEquals(TEST_ETLD_PLUS_ONE_2, headerModel.get(IDP_FOR_DISPLAY));
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
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");

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
                        Color.BLACK, Color.BLACK, "", TEST_CONFIG_URL, TEST_LOGIN_URL);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                idpMetadataNoBrandIconUrl,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertNull(headerModel.get(IDP_BRAND_ICON));

        // The only downloaded icon should not be an IDP brand icon.
        verify(mMockImageFetcher, times(1))
                .fetchImage(
                        argThat(imageFetcherParamsHaveUrl(TEST_PROFILE_PIC)), any(Callback.class));
    }

    @Test
    public void testShowAccountSignUpHeader() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(NEW_USER),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountsSetsAccountListAndRequestsAvatar() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
        assertNull(mSheetAccountItems.get(0).model.get(AVATAR));
        assertNull(mSheetAccountItems.get(1).model.get(AVATAR));

        // Both accounts have the same profile pic URL
        ImageFetcher.Params expected_params =
                ImageFetcher.Params.create(
                        TEST_PROFILE_PIC.getSpec(),
                        ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME,
                        DESIRED_AVATAR_SIZE,
                        DESIRED_AVATAR_SIZE);

        verify(mMockImageFetcher, times(2)).fetchImage(eq(expected_params), any());
    }

    @Test
    public void testFetchAvatarUpdatesModel() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Collections.singletonList(CARL),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertEquals("Incorrect account", CARL, mSheetAccountItems.get(0).model.get(ACCOUNT));
        assertNull(mSheetAccountItems.get(0).model.get(AVATAR));

        ImageFetcher.Params expected_params =
                ImageFetcher.Params.create(
                        TEST_PROFILE_PIC.getSpec(),
                        ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME,
                        DESIRED_AVATAR_SIZE,
                        DESIRED_AVATAR_SIZE);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mMockImageFetcher).fetchImage(eq(expected_params), callback.capture());

        Bitmap bitmap =
                Bitmap.createBitmap(
                        DESIRED_AVATAR_SIZE, DESIRED_AVATAR_SIZE, Bitmap.Config.ARGB_8888);
        callback.getValue().onResult(bitmap);

        Avatar avatarData = mSheetAccountItems.get(0).model.get(AVATAR);
        assertEquals("incorrect avatar bitmap", bitmap, avatarData.mAvatar);
        assertEquals("incorrect avatar name", CARL.getName(), avatarData.mName);
        assertEquals("incorrect avatar size", DESIRED_AVATAR_SIZE, avatarData.mAvatarSize);
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        assertEquals(3, countAllItems()); // Header + two Accounts
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Collections.singletonList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals("Incorrect account", ANA, mSheetAccountItems.get(0).model.get(ACCOUNT));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Collections.singletonList(BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals("Incorrect account", BOB, mSheetAccountItems.get(0).model.get(ACCOUNT));
    }

    @Test
    public void testShowAccountsSetsVisible() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, CARL, BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        verify(mMockBottomSheetController, times(1)).requestShowContent(any(), eq(true));

        assertFalse(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
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
                .onResult(ANA);
        verify(mMockDelegate).onAccountSelected(TEST_CONFIG_URL, ANA);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, CARL),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        // Do not let test inputs be ignored.
        mMediator.setComponentShowTime(-1000);
        assertFalse(mMediator.wasDismissed());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));

        mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onAccountSelected(TEST_CONFIG_URL, CARL);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnSingleAccountDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerSelectSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, BOB),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(TEST_CONFIG_URL, ANA);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testShowsTosOnMultiAccountSelectSignUp() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, NEW_USER),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        mMediator.onAccountSelected(NEW_USER);

        assertFalse(mMediator.wasDismissed());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(1, mSheetAccountItems.size());

        verify(mMockDelegate, never()).onAccountSelected(TEST_CONFIG_URL, NEW_USER);
    }

    @Test
    public void testShowsAccountPickerOnTosDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA, NEW_USER),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        mMediator.onAccountSelected(NEW_USER);

        pressBack();
        assertFalse(mMediator.wasDismissed());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(2, mSheetAccountItems.size());

        pressBack();
        assertTrue(mMediator.wasDismissed());

        verify(mMockDelegate, never()).onAccountSelected(TEST_CONFIG_URL, NEW_USER);
    }

    @Test
    public void testCallsDelegateAndHidesOnAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ true,
                /* rpContext= */ "signin");
        // Auto reauthenticates if no action is taken.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockDelegate).onAccountSelected(TEST_CONFIG_URL, ANA);
        assertFalse(mMediator.wasDismissed());
        mMediator.close();
        assertTrue(mMediator.wasDismissed());
    }

    @Test
    public void testCallsDelegateAndHidesOnlyOnceWithAutoReauthn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ true,
                /* rpContext= */ "signin");
        // Auto reauthenticates even if dismissed.
        pressBack();
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
        verify(mMockDelegate).onAccountSelected(TEST_CONFIG_URL, ANA);
        verifyNoMoreInteractions(mMockDelegate);
        assertTrue(mMediator.wasDismissed());
        // The delayed task should not call delegate after user dismissing.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testShowDataSharingConsentForSingleNewAccount() {
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(NEW_USER),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        // For new user we expect header + account + consent text + continue btn
        assertEquals(4, countAllItems());
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        DataSharingConsentProperties.Properties dataSharingProperties =
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES);
        assertEquals(
                "Incorrect privacy policy URL",
                TEST_URL_PRIVACY_POLICY,
                dataSharingProperties.mPrivacyPolicyUrl);
        assertEquals(
                "Incorrect terms of service URL",
                TEST_URL_TERMS_OF_SERVICE,
                dataSharingProperties.mTermsOfServiceUrl);
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        assertEquals(
                "Incorrect provider ETLD+1",
                TEST_ETLD_PLUS_ONE_2,
                dataSharingProperties.mIdpForDisplay);
    }

    @Test
    public void testShowVerifySheetExplicitSignin() {
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showAccounts(
                    TEST_ETLD_PLUS_ONE,
                    TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2,
                    Arrays.asList(NEW_USER),
                    IDP_METADATA,
                    CLIENT_ID_METADATA,
                    /* isAutoReauthn= */ false,
                    rpContext);
            mMediator.showVerifySheet(ANA);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
        }
    }

    @Test
    public void testShowVerifySheetAutoReauthn() {
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            // showVerifySheet is called in showAccounts when isAutoReauthn is true
            mMediator.showAccounts(
                    TEST_ETLD_PLUS_ONE,
                    TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2,
                    Arrays.asList(ANA),
                    IDP_METADATA,
                    CLIENT_ID_METADATA,
                    /* isAutoReauthn= */ true,
                    rpContext);

            assertEquals(1, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.VERIFY_AUTO_REAUTHN, mModel.get(ItemProperties.HEADER).get(TYPE));
        }
    }

    @Test
    public void testShowFailureDialog() {
        int count = 0;
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showFailureDialog(
                    TEST_ETLD_PLUS_ONE,
                    TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2,
                    IDP_METADATA,
                    rpContext);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(
                    HeaderType.SIGN_IN_TO_IDP_STATIC, mModel.get(ItemProperties.HEADER).get(TYPE));
            // For failure dialog, we expect header + IDP sign in text + continue btn
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.IDP_SIGNIN));

            String idpEtldPlusOne =
                    mModel.get(ItemProperties.IDP_SIGNIN).get(IdpSignInProperties.IDP_FOR_DISPLAY);
            assertEquals("Incorrect provider ETLD+1", TEST_ETLD_PLUS_ONE_2, idpEtldPlusOne);

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
            verify(mMockDelegate, times(++count)).onLoginToIdP(TEST_LOGIN_URL);
        }
    }

    @Test
    public void testShowErrorDialogClickGotIt() {
        int count = 0;
        for (String rpContext : RP_CONTEXTS) {
            when(mMockBottomSheetController.requestShowContent(any(), anyBoolean()))
                    .thenReturn(true);
            mMediator.showErrorDialog(
                    TEST_ETLD_PLUS_ONE,
                    TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2,
                    IDP_METADATA,
                    rpContext,
                    TOKEN_ERROR_EMPTY_URL);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.SIGN_IN_ERROR, mModel.get(ItemProperties.HEADER).get(TYPE));

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1",
                    TEST_ETLD_PLUS_ONE_2,
                    errorProperties.mIdpForDisplay);
            assertEquals(
                    "Incorrect top frame ETLD+1",
                    TEST_ETLD_PLUS_ONE,
                    errorProperties.mTopFrameForDisplay);
            assertEquals("Incorrect token error", TOKEN_ERROR_EMPTY_URL, errorProperties.mError);

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
                    .onResult(ANA);
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
                    TEST_ETLD_PLUS_ONE,
                    TEST_ETLD_PLUS_ONE_1,
                    TEST_ETLD_PLUS_ONE_2,
                    IDP_METADATA,
                    rpContext,
                    TOKEN_ERROR);
            assertEquals(0, mSheetAccountItems.size());
            assertEquals(HeaderType.SIGN_IN_ERROR, mModel.get(ItemProperties.HEADER).get(TYPE));

            // For error dialog, we expect header + error text + got it button
            assertEquals(3, countAllItems());
            assertTrue(containsItemOfType(mModel, ItemProperties.ERROR_TEXT));

            ErrorProperties.Properties errorProperties =
                    mModel.get(ItemProperties.ERROR_TEXT).get(ErrorProperties.PROPERTIES);
            assertEquals(
                    "Incorrect provider ETLD+1",
                    TEST_ETLD_PLUS_ONE_2,
                    errorProperties.mIdpForDisplay);
            assertEquals(
                    "Incorrect top frame ETLD+1",
                    TEST_ETLD_PLUS_ONE,
                    errorProperties.mTopFrameForDisplay);
            assertEquals("Incorrect token error", TOKEN_ERROR, errorProperties.mError);

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
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
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
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
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
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
        // We pass null as |mMediatior| does not really care about where we navigate to.
        mMediator.getTabObserver().onDidStartNavigationInPrimaryMainFrame(mTab, null);
        assertTrue(mMediator.wasDismissed());
        verify(mMockDelegate).onDismissed(IdentityRequestDialogDismissReason.OTHER);
    }

    @Test
    public void testShowKeyboardWhileNotInteractable() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(
                TEST_ETLD_PLUS_ONE,
                TEST_ETLD_PLUS_ONE_1,
                TEST_ETLD_PLUS_ONE_2,
                Arrays.asList(ANA),
                IDP_METADATA,
                CLIENT_ID_METADATA,
                /* isAutoReauthn= */ false,
                /* rpContext= */ "signin");
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
