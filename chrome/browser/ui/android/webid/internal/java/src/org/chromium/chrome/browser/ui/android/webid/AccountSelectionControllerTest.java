// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties.SINGLE_ACCOUNT;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemType;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;

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

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private AccountSelectionComponent.Delegate mMockDelegate;
    @Mock
    private BottomSheetController mMockBottomSheetController;

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

        mMediator = new AccountSelectionMediator(
                mMockDelegate, mSheetItems, mMockBottomSheetController, null);
    }

    @Test
    public void testShowAccountsCreatesHeader() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        assertThat("Incorrect header type", mSheetItems.get(0).type, is(ItemType.HEADER));
        assertThat("Incorrect header multiple accounts",
                mSheetItems.get(0).model.get(SINGLE_ACCOUNT), is(false));
        assertThat("Incorrect header url", mSheetItems.get(0).model.get(FORMATTED_URL),
                is(formatForSecurityDisplay(TEST_URL)));
    }

    @Test
    public void testShowAccountWithSingleEntryCreatesHeader() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA));
        assertThat("Incorrect header type", mSheetItems.get(0).type, is(ItemType.HEADER));
        assertThat("Incorrect header single account", mSheetItems.get(0).model.get(SINGLE_ACCOUNT),
                is(true));
        assertThat("Incorrect header url", mSheetItems.get(0).model.get(FORMATTED_URL),
                is(formatForSecurityDisplay(TEST_URL)));
    }

    @Test
    public void testShowAccountsSetsVisibile() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, CARL, BOB));
        verify(mMockBottomSheetController, times(1)).requestShowContent(eq(null), eq(true));

        assertThat("Incorrect visibility", mMediator.isVisible(), is(true));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertThat("Incorrect visibility", mMediator.isVisible(), is(false));
    }

    @Test
    public void testCallsDelegateAndHidesOnSelect() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertThat("Incorrect visibility", mMediator.isVisible(), is(false));
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
