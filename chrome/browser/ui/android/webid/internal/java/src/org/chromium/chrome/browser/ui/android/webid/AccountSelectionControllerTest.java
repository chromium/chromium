// Copyright 2021 The Chromium Authors. All rights reserved.
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
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ACCOUNT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.AVATAR;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_IDP_ETLD_PLUS_ONE;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_RP_ETLD_PLUS_ONE;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.IDP_BRAND_ICON;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.TYPE;

import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.Avatar;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AutoSignInCancelButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.HeaderType;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.ClientIdMetadata;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.Arrays;
import java.util.Collections;

/**
 * Controller tests verify that the Account Selection delegate modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountSelectionControllerTest {
    // Note that these are not actual ETLD+1 values, but this is irrelevant for the purposes of this
    // test.
    private static final String TEST_ETLD_PLUS_ONE = JUnitTestGURLs.EXAMPLE_URL;
    private static final String TEST_ETLD_PLUS_ONE_1 = JUnitTestGURLs.URL_1;
    private static final String TEST_ETLD_PLUS_ONE_2 = JUnitTestGURLs.URL_2;
    private static final GURL TEST_PROFILE_PIC =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1_WITH_PATH);
    private static final GURL TEST_URL_TERMS_OF_SERVICE =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
    private static final GURL TEST_URL_PRIVACY_POLICY =
            JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_2);

    private static final Account ANA = new Account(
            "Ana", "ana@one.test", "Ana Doe", "Ana", TEST_PROFILE_PIC, /*isSignIn=*/true);
    private static final Account BOB =
            new Account("Bob", "", "Bob", "", TEST_PROFILE_PIC, /*isSignIn=*/true);
    private static final Account CARL = new Account(
            "Carl", "carl@three.test", "Carl Test", ":)", TEST_PROFILE_PIC, /*isSignIn=*/true);
    private static final Account NEW_USER = new Account("602214076", "goto@email.example",
            "Sam E. Goto", "Sam", TEST_PROFILE_PIC, /*isSignIn=*/false);

    private static final ClientIdMetadata CLIENT_ID_METADATA =
            new ClientIdMetadata(TEST_URL_TERMS_OF_SERVICE, TEST_URL_PRIVACY_POLICY);

    private static final @Px int DESIRED_AVATAR_SIZE = 100;

    // Needs Bitmap.class Mockito mock for initialization. Initialized in
    // AccountSelectionControllerTest constructor.
    public final IdentityProviderMetadata IDP_METADATA;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private AccountSelectionComponent.Delegate mMockDelegate;
    @Mock
    private ImageFetcher mMockImageFetcher;
    @Mock
    private BottomSheetController mMockBottomSheetController;

    private AccountSelectionBottomSheetContent mBottomSheetContent;
    private AccountSelectionMediator mMediator;
    private final PropertyModel mModel =
            new PropertyModel.Builder(AccountSelectionProperties.ItemProperties.ALL_KEYS).build();
    private final ModelList mSheetAccountItems = new ModelList();

    public AccountSelectionControllerTest() {
        MockitoAnnotations.initMocks(this);
        IDP_METADATA = new IdentityProviderMetadata(Color.BLACK, Color.BLACK, mock(Bitmap.class));
    }

    @Before
    public void setUp() {
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitScheme(anyString()))
                .then(inv -> format(inv.getArgument(0)));
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> formatForSecurityDisplay(inv.getArgument(0)));
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(
                     any(GURL.class), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> formatForSecurityDisplay(((GURL) inv.getArgument(0)).getSpec()));
        when(mUrlFormatterJniMock.fixupUrl(anyString())).then(inv -> fixupUrl(inv.getArgument(0)));

        mBottomSheetContent = new AccountSelectionBottomSheetContent(null, null);
        mMediator = new AccountSelectionMediator(mMockDelegate, mModel, mSheetAccountItems,
                mMockBottomSheetController, mBottomSheetContent, mMockImageFetcher,
                DESIRED_AVATAR_SIZE);
    }

    @Test
    public void testShowAccountSignInHeader() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, false /* isAutoSignIn */);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
        assertEquals(formatForSecurityDisplay(TEST_ETLD_PLUS_ONE),
                headerModel.get(FORMATTED_RP_ETLD_PLUS_ONE));
        assertEquals(formatForSecurityDisplay(TEST_ETLD_PLUS_ONE_1),
                headerModel.get(FORMATTED_IDP_ETLD_PLUS_ONE));
        assertNotNull(headerModel.get(IDP_BRAND_ICON));
    }

    @Test
    public void testShowAccountSignUpHeader() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(NEW_USER),
                IDP_METADATA, CLIENT_ID_METADATA, false /* isAutoSignIn */);

        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountAutoSignInHeader() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, true /* isAutoSignIn */);
        PropertyModel headerModel = mModel.get(ItemProperties.HEADER);
        assertEquals(HeaderType.AUTO_SIGN_IN, headerModel.get(TYPE));
    }

    @Test
    public void testShowAccountsSetsAccountListAndRequestsAvatar() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA, BOB),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
        assertNull(mSheetAccountItems.get(0).model.get(AVATAR));
        assertNull(mSheetAccountItems.get(1).model.get(AVATAR));

        // Both accounts have the same profile pic URL
        ImageFetcher.Params expected_params = ImageFetcher.Params.create(TEST_PROFILE_PIC.getSpec(),
                ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME, DESIRED_AVATAR_SIZE,
                DESIRED_AVATAR_SIZE);

        verify(mMockImageFetcher, times(2)).fetchImage(eq(expected_params), any());
    }

    @Test
    public void testFetchAvatarUpdatesModel() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Collections.singletonList(CARL), IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertEquals("Incorrect account", CARL, mSheetAccountItems.get(0).model.get(ACCOUNT));
        assertNull(mSheetAccountItems.get(0).model.get(AVATAR));

        ImageFetcher.Params expected_params = ImageFetcher.Params.create(TEST_PROFILE_PIC.getSpec(),
                ImageFetcher.WEB_ID_ACCOUNT_SELECTION_UMA_CLIENT_NAME, DESIRED_AVATAR_SIZE,
                DESIRED_AVATAR_SIZE);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mMockImageFetcher).fetchImage(eq(expected_params), callback.capture());

        Bitmap bitmap = Bitmap.createBitmap(
                DESIRED_AVATAR_SIZE, DESIRED_AVATAR_SIZE, Bitmap.Config.ARGB_8888);
        callback.getValue().onResult(bitmap);

        Avatar avatarData = mSheetAccountItems.get(0).model.get(AVATAR);
        assertEquals("incorrect avatar bitmap", bitmap, avatarData.mAvatar);
        assertEquals("incorrect avatar name", CARL.getName(), avatarData.mName);
        assertEquals("incorrect avatar size", DESIRED_AVATAR_SIZE, avatarData.mAvatarSize);
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA, BOB),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals(3, countAllItems()); // Header + two Accounts
        assertEquals("Incorrect item sheet count", 2, mSheetAccountItems.size());
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Collections.singletonList(ANA), IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals("Incorrect account", ANA, mSheetAccountItems.get(0).model.get(ACCOUNT));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Collections.singletonList(BOB), IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals(3, countAllItems()); // Header + Account + Continue Button
        assertEquals(1, mSheetAccountItems.size());
        assertEquals("Incorrect account", BOB, mSheetAccountItems.get(0).model.get(ACCOUNT));
    }

    @Test
    public void testShowAccountsSetsVisible() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Arrays.asList(ANA, CARL, BOB), IDP_METADATA, CLIENT_ID_METADATA, false);
        verify(mMockBottomSheetController, times(1)).requestShowContent(any(), eq(true));

        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
        assertNotNull(mModel.get(ItemProperties.CONTINUE_BUTTON)
                              .get(ContinueButtonProperties.ON_CLICK_LISTENER));

        mModel.get(ItemProperties.CONTINUE_BUTTON)
                .get(ContinueButtonProperties.ON_CLICK_LISTENER)
                .onResult(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertEquals(true, mMediator.isVisible());
        mMediator.hideBottomSheet();
        assertEquals(false, mMediator.isVisible());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA, CARL),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
        assertNotNull(mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER));

        mSheetAccountItems.get(0).model.get(AccountProperties.ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onAccountSelected(CARL);
        assertEquals(true, mMediator.isVisible());
        mMediator.hideBottomSheet();
        assertEquals(false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnSingleAccountDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        pressBack();
        verify(mMockDelegate).onDismissed();
        assertEquals(false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA, BOB),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        pressBack();
        verify(mMockDelegate).onDismissed();
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnAccountPickerSelectSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA, BOB),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertEquals(true, mMediator.isVisible());
        mMediator.hideBottomSheet();
        assertEquals(false, mMediator.isVisible());
    }

    @Test
    public void testShowsTosOnMultiAccountSelectSignUp() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Arrays.asList(ANA, NEW_USER), IDP_METADATA, CLIENT_ID_METADATA, false);
        mMediator.onAccountSelected(NEW_USER);

        assertEquals(true, mMediator.isVisible());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(1, mSheetAccountItems.size());

        verify(mMockDelegate, never()).onAccountSelected(NEW_USER);
    }

    @Test
    public void testShowsAccountPickerOnTosDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1,
                Arrays.asList(ANA, NEW_USER), IDP_METADATA, CLIENT_ID_METADATA, false);
        mMediator.onAccountSelected(NEW_USER);

        pressBack();
        assertTrue(mMediator.isVisible());
        assertFalse(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));
        assertEquals(2, mSheetAccountItems.size());

        pressBack();
        assertFalse(mMediator.isVisible());

        verify(mMockDelegate, never()).onAccountSelected(NEW_USER);
    }

    @Test
    public void testCallsDelegateAndHidesOnAutoSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, true);
        // Auto signs in if no action is taken.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mMockDelegate).onAccountSelected(ANA);
        assertEquals(true, mMediator.isVisible());
        mMediator.hideBottomSheet();
        assertEquals(false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnCancellingAutoSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, true);
        mMediator.onAutoSignInCancelled();
        verify(mMockDelegate).onAutoSignInCancelled();
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsCallbackAndHidesOnCancellingAutoSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, true);
        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
        assertNotNull(mModel.get(ItemProperties.AUTO_SIGN_IN_CANCEL_BUTTON)
                              .get(AutoSignInCancelButtonProperties.ON_CLICK_LISTENER));

        mModel.get(ItemProperties.AUTO_SIGN_IN_CANCEL_BUTTON)
                .get(AutoSignInCancelButtonProperties.ON_CLICK_LISTENER)
                .run();
        verify(mMockDelegate).onAutoSignInCancelled();
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnlyOnceWithAutoSignIn() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_1, Arrays.asList(ANA),
                IDP_METADATA, CLIENT_ID_METADATA, true);
        pressBack();
        verify(mMockDelegate).onDismissed();
        verifyNoMoreInteractions(mMockDelegate);
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
        // The delayed task should not call delegate after user dismissing.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
    }

    @Test
    public void testShowDataSharingConsentForSingleNewAccount() {
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, Arrays.asList(NEW_USER),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        // For new user we expect header + account + consent text + continue btn
        assertEquals(4, countAllItems());
        assertEquals("Incorrect item sheet count", 1, mSheetAccountItems.size());
        assertTrue(containsItemOfType(mModel, ItemProperties.DATA_SHARING_CONSENT));

        DataSharingConsentProperties.Properties dataSharingProperties =
                mModel.get(ItemProperties.DATA_SHARING_CONSENT)
                        .get(DataSharingConsentProperties.PROPERTIES);
        assertEquals("Incorrect privacy policy URL", TEST_URL_PRIVACY_POLICY.getSpec(),
                dataSharingProperties.mPrivacyPolicyUrl);
        assertEquals("Incorrect terms of service URL", TEST_URL_TERMS_OF_SERVICE.getSpec(),
                dataSharingProperties.mTermsOfServiceUrl);
        assertTrue(containsItemOfType(mModel, ItemProperties.CONTINUE_BUTTON));
        assertEquals("Incorrect provider ETLD+1", formatForSecurityDisplay(TEST_ETLD_PLUS_ONE_2),
                dataSharingProperties.mFormattedIdpEtldPlusOne);
    }

    @Test
    public void testShowVerifySheet() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_ETLD_PLUS_ONE, TEST_ETLD_PLUS_ONE_2, Arrays.asList(NEW_USER),
                IDP_METADATA, CLIENT_ID_METADATA, false);
        mMediator.showVerifySheet(ANA);

        assertEquals(1, mSheetAccountItems.size());
        assertEquals(HeaderType.VERIFY, mModel.get(ItemProperties.HEADER).get(TYPE));
    }

    private void pressBack() {
        if (mBottomSheetContent.handleBackPress()) return;

        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
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

    /**
     * Helper to verify formatted URLs. The real implementation calls {@link UrlFormatter}. It's not
     * useful to actually reimplement the formatter, so just modify the string in a trivial way.
     * @param originUrl A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String format(String originUrl) {
        return "formatted_" + originUrl + "_formatted";
    }

    /**
     * Helper to verify URLs formatted for security display. The real implementation calls
     * {@link UrlFormatter}. It's not useful to actually reimplement the formatter, so just
     * modify the string in a trivial way.
     * @param url A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String formatForSecurityDisplay(String url) {
        return "formatted_for_security_" + url + "_formatted_for_security";
    }

    /**
     * Helper to verify URLs formatted for security display. The real implementation calls
     * {@link UrlFormatter}. It's not useful to actually reimplement the formatter, so just
     * modify the string in a trivial way.
     * @param url A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static GURL fixupUrl(String url) {
        return new GURL(url);
    }
}
