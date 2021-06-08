// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
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
            new Account("Ana", "S3cr3t", "Ana Doe", "Ana", TEST_PROFILE_PIC, TEST_URL);
    private static final Account BOB =
            new Account("Bob", "*****", "Bob", "", TEST_PROFILE_PIC, TEST_SUBDOMAIN_URL);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private AccountSelectionComponent.Delegate mMockDelegate;

    @Mock
    private BottomSheetController mMockBottomSheetController;

    private AccountSelectionMediator mMediator;
    private final ModelList mSheetItems = new ModelList();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator = new AccountSelectionMediator(
                mMockDelegate, mSheetItems, mMockBottomSheetController, null);
    }

    @Test
    public void testShowAccountsSetsVisibile() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        assertThat(mMediator.isVisible(), is(true));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertThat(mMediator.isVisible(), is(false));
    }

    @Test
    public void testCallsDelegateAndHidesOnSelect() {
        when(mMockBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertThat(mMediator.isVisible(), is(false));
    }
}
