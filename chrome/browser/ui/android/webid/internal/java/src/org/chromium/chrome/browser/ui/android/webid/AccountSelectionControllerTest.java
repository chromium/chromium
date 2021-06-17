// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ACCOUNT;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SINGLE_ACCOUNT;

import android.graphics.Bitmap;

import androidx.annotation.Px;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties.FaviconOrFallback;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;
import java.util.Collections;

/**
 * Controller tests verify that the Account Selection delegate modifies the model if the API is used
 * properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccountSelectionControllerTest {
    private static final String TEST_URL = "https://www.example.xyz";
    private static final String TEST_SUBDOMAIN_URL = "https://subdomain.example.xyz";
    private static final String TEST_PROFILE_PIC = "https://www.example.xyz/profile/2";

    private static final Account ANA =
            new Account("Ana", "S3cr3t", "Ana Doe", "Ana", TEST_PROFILE_PIC, "https://m.a.xyz/");
    private static final Account BOB =
            new Account("Bob", "*****", "Bob", "", TEST_PROFILE_PIC, TEST_SUBDOMAIN_URL);
    private static final Account CARL =
            new Account("Carl", "G3h3!m", "Carl Test", ":)", TEST_PROFILE_PIC, TEST_URL);
    private static final @Px int DESIRED_FAVICON_SIZE = 64;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private AccountSelectionComponent.Delegate mMockDelegate;
    @Mock
    private LargeIconBridge mMockIconBridge;
    @Mock
    private BottomSheetController mMockBottomSheetController;

    // Can't be local, as it has to be initialized by initMocks.
    @Captor
    private ArgumentCaptor<LargeIconBridge.LargeIconCallback> mCallbackArgumentCaptor;

    private AccountSelectionMediator mMediator;
    private final ModelList mSheetItems = new ModelList();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForDisplayOmitScheme(anyString()))
                .then(inv -> format(inv.getArgument(0)));
        when(mUrlFormatterJniMock.formatStringUrlForSecurityDisplay(
                     anyString(), eq(SchemeDisplay.OMIT_HTTP_AND_HTTPS)))
                .then(inv -> formatForSecurityDisplay(inv.getArgument(0)));

        mMediator = new AccountSelectionMediator(mMockDelegate, mSheetItems,
                mMockBottomSheetController, null, mMockIconBridge, DESIRED_FAVICON_SIZE);
    }

    @Test
    public void testShowAccountsCreatesHeader() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        assertEquals("Incorrect header type", ItemType.HEADER, mSheetItems.get(0).type);
        assertEquals("Incorrect header multiple accounts", false,
                mSheetItems.get(0).model.get(SINGLE_ACCOUNT));
        assertEquals("Incorrect header url", formatForSecurityDisplay(TEST_URL),
                mSheetItems.get(0).model.get(FORMATTED_URL));
    }

    @Test
    public void testShowAccountWithSingleEntryCreatesHeader() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA));
        assertEquals("Incorrect header type", ItemType.HEADER, mSheetItems.get(0).type);
        assertEquals("Incorrect header single account", true,
                mSheetItems.get(0).model.get(SINGLE_ACCOUNT));
        assertEquals("Incorrect header url", formatForSecurityDisplay(TEST_URL),
                mSheetItems.get(0).model.get(FORMATTED_URL));
    }

    @Test
    public void testShowAccountsSetsAccountListAndRequestsFavicons() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        assertEquals(
                "Incorrect item sheet count", 4, mSheetItems.size()); // Header + three Accounts
        assertEquals("Incorrect type", ItemType.ACCOUNT, mSheetItems.get(1).type);
        assertEquals("Incorrect account", ANA, mSheetItems.get(1).model.get(ACCOUNT));
        assertNull(mSheetItems.get(1).model.get(FAVICON_OR_FALLBACK));
        assertEquals("Incorrect type", ItemType.ACCOUNT, mSheetItems.get(2).type);
        assertEquals("Incorrect account", CARL, mSheetItems.get(2).model.get(ACCOUNT));
        assertNull(mSheetItems.get(2).model.get(FAVICON_OR_FALLBACK));
        assertEquals("Incorrect type", ItemType.ACCOUNT, mSheetItems.get(3).type);
        assertEquals("Incorrect account", BOB, mSheetItems.get(3).model.get(ACCOUNT));
        assertNull(mSheetItems.get(3).model.get(FAVICON_OR_FALLBACK));

        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(CARL.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(ANA.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(eq(BOB.getOriginUrl()), eq(DESIRED_FAVICON_SIZE), any());
    }

    @Test
    public void testFetchFaviconUpdatesModel() {
        mMediator.showAccounts(TEST_URL, Collections.singletonList(CARL));
        assertEquals("Incorrect item sheet count", 3,
                mSheetItems.size()); // Header + Account + Continue Button
        assertEquals("Incorrect type", ItemType.ACCOUNT, mSheetItems.get(1).type);
        assertEquals("Incorrect account", CARL, mSheetItems.get(1).model.get(ACCOUNT));
        assertNull(mSheetItems.get(1).model.get(FAVICON_OR_FALLBACK));

        // ANA and CARL both have TEST_URL as their origin URL
        verify(mMockIconBridge)
                .getLargeIconForStringUrl(
                        eq(TEST_URL), eq(DESIRED_FAVICON_SIZE), mCallbackArgumentCaptor.capture());
        LargeIconBridge.LargeIconCallback callback = mCallbackArgumentCaptor.getValue();
        Bitmap bitmap = Bitmap.createBitmap(
                DESIRED_FAVICON_SIZE, DESIRED_FAVICON_SIZE, Bitmap.Config.ARGB_8888);
        callback.onLargeIconAvailable(bitmap, 333, true, IconType.FAVICON);
        FaviconOrFallback iconData = mSheetItems.get(1).model.get(FAVICON_OR_FALLBACK);
        assertEquals("incorrect favicon bitmap", bitmap, iconData.mIcon);
        assertEquals("incorrect favicon url", TEST_URL, iconData.mUrl);
        assertEquals("incorrect favicon size", DESIRED_FAVICON_SIZE, iconData.mIconSize);
        assertEquals("incorrect favicon fallback color", 333, iconData.mFallbackColor);
    }

    @Test
    public void testShowAccountsFormatPslOrigins() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        assertEquals("Incorrect item sheet count", 3, mSheetItems.size()); // Header + two Accounts
        assertEquals("Incorrect item type", ItemType.ACCOUNT, mSheetItems.get(1).type);
        assertEquals("Incorrect item type", ItemType.ACCOUNT, mSheetItems.get(2).type);
    }

    @Test
    public void testClearsAccountListWhenShowingAgain() {
        mMediator.showAccounts(TEST_URL, Collections.singletonList(ANA));
        assertEquals("Incorrect item sheet count", 3,
                mSheetItems.size()); // Header + Account + Continue Button
        assertEquals("Incorrect item type", ItemType.ACCOUNT, mSheetItems.get(1).type);
        assertEquals("Incorrect account", ANA, mSheetItems.get(1).model.get(ACCOUNT));
        assertNull(mSheetItems.get(1).model.get(FAVICON_OR_FALLBACK));

        // Showing the sheet a second time should replace all changed accounts.
        mMediator.showAccounts(TEST_URL, Collections.singletonList(BOB));
        assertEquals("Incorrect item sheet count", 3,
                mSheetItems.size()); // Header + Account + Continue Button
        assertEquals("Incorrect item type", ItemType.ACCOUNT, mSheetItems.get(1).type);
        assertEquals("Incorrect account", BOB, mSheetItems.get(1).model.get(ACCOUNT));
        assertNull(mSheetItems.get(1).model.get(FAVICON_OR_FALLBACK));
    }

    @Test
    public void testShowAccountsSetsVisibile() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        verify(mMockBottomSheetController, times(1)).requestShowContent(eq(null), eq(true));

        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItemDoesNotRecordIndexForSingleAccount() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA));
        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
        assertNotNull(mSheetItems.get(1).model.get(ON_CLICK_LISTENER));

        mSheetItems.get(1).model.get(ON_CLICK_LISTENER).onResult(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsCallbackAndHidesOnSelectingItem() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, CARL));
        assertEquals("Incorrectly hidden", true, mMediator.isVisible());
        assertNotNull(mSheetItems.get(1).model.get(ON_CLICK_LISTENER));

        mSheetItems.get(1).model.get(ON_CLICK_LISTENER).onResult(CARL);
        verify(mMockDelegate).onAccountSelected(CARL);
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
    }

    @Test
    public void testCallsDelegateAndHidesOnSelect() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertEquals("Incorrectly visible", false, mMediator.isVisible());
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
     * @param originUrl A URL {@link String} to "format".
     * @return A "formatted" URL {@link String}.
     */
    private static String formatForSecurityDisplay(String originUrl) {
        return "formatted_for_security_" + originUrl + "_formatted_for_security";
    }
}
