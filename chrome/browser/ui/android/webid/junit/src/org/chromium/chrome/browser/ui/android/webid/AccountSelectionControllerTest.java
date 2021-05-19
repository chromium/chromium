// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.VISIBLE;

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
import org.chromium.ui.modelutil.PropertyModel;

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

    private final AccountSelectionMediator mMediator = new AccountSelectionMediator();
    private final PropertyModel mModel =
            AccountSelectionProperties.createDefaultModel(mMediator::onDismissed);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMediator.initialize(mMockDelegate, mModel);
    }

    @Test
    public void testCreatesValidDefaultModel() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testShowAccountsSetsVisibile() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testCallsDelegateAndHidesOnDismiss() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onDismissed(BottomSheetController.StateChangeReason.BACK_PRESS);
        verify(mMockDelegate).onDismissed();
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testCallsDelegateAndHidesOnSelect() {
        mMediator.showAccounts(TEST_URL, Arrays.asList(ANA, BOB));
        mMediator.onAccountSelected(ANA);
        verify(mMockDelegate).onAccountSelected(ANA);
        assertThat(mModel.get(VISIBLE), is(false));
    }
}
