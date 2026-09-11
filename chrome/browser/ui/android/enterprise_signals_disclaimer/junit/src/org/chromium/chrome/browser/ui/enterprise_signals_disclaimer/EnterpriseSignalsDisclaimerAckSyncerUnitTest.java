// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link EnterpriseSignalsDisclaimerAckSyncer}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
public class EnterpriseSignalsDisclaimerAckSyncerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private EnterpriseSignalsDisclaimerBridge.Natives mBridgeNativesMock;
    @Mock private Profile mProfile;

    @Captor private ArgumentCaptor<List<GaiaId>> mAccountsCaptor;

    @Before
    public void setUp() {
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(mBridgeNativesMock);
    }

    @After
    public void tearDown() {
        EnterpriseSignalsDisclaimerAckSyncer.resetForTesting();
        EnterpriseSignalsDisclaimerBridgeJni.setInstanceForTesting(null);
    }

    @Test
    public void testInitialize_withAccounts_syncsAccounts() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);

        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);

        verify(mBridgeNativesMock).removeUnknownAccounts(mAccountsCaptor.capture());
        Assert.assertEquals(
                List.of(TestAccounts.ACCOUNT1.getGaiaId(), TestAccounts.ACCOUNT2.getGaiaId()),
                mAccountsCaptor.getValue());
    }

    @Test
    public void testInitialize_noAccounts_syncsEmptyList() {
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);

        verify(mBridgeNativesMock).removeUnknownAccounts(Collections.emptyList());
    }

    @Test
    public void testInitialize_refreshTokensNotLoaded_doesNotSyncUntilLoaded() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setAreRefreshTokensLoaded(false);

        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);

        verify(mBridgeNativesMock, never()).removeUnknownAccounts(any());

        mAccountManagerTestRule.getIdentityManager().setAreRefreshTokensLoaded(true);

        verify(mBridgeNativesMock).removeUnknownAccounts(mAccountsCaptor.capture());
        Assert.assertEquals(List.of(TestAccounts.ACCOUNT1.getGaiaId()), mAccountsCaptor.getValue());
    }

    @Test
    public void testInitialize_calledMultipleTimes_onlyInitializesOnce() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);

        verify(mBridgeNativesMock, times(1)).removeUnknownAccounts(any());
    }

    @Test
    public void testOnRefreshTokenRemoved_syncsUpdatedAccounts() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);
        verify(mBridgeNativesMock)
                .removeUnknownAccounts(
                        List.of(
                                TestAccounts.ACCOUNT1.getGaiaId(),
                                TestAccounts.ACCOUNT2.getGaiaId()));
        clearInvocations(mBridgeNativesMock);

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // We pass the list of accounts still on device to `removeUnknownAccounts`, so that the
        // native can clear the acknowledgments for the removed accounts.
        verify(mBridgeNativesMock).removeUnknownAccounts(mAccountsCaptor.capture());
        Assert.assertEquals(List.of(TestAccounts.ACCOUNT2.getGaiaId()), mAccountsCaptor.getValue());
    }

    @Test
    public void testOnRefreshTokenRemoved_whenRefreshTokensNotLoaded_doesNotSync() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);
        verify(mBridgeNativesMock).removeUnknownAccounts(any());
        clearInvocations(mBridgeNativesMock);

        mAccountManagerTestRule.getIdentityManager().setAreRefreshTokensLoaded(false);
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mBridgeNativesMock, never()).removeUnknownAccounts(any());
    }

    @Test
    public void testAccountAdded_doesNotSync() {
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);
        verify(mBridgeNativesMock).removeUnknownAccounts(Collections.emptyList());
        clearInvocations(mBridgeNativesMock);

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        verify(mBridgeNativesMock, never()).removeUnknownAccounts(any());
    }

    @Test
    public void testResetForTesting_unregistersObserver() {
        EnterpriseSignalsDisclaimerAckSyncer.initialize(mProfile);
        Assert.assertEquals(1, mAccountManagerTestRule.getIdentityManager().getObserverCount());

        EnterpriseSignalsDisclaimerAckSyncer.resetForTesting();

        Assert.assertEquals(0, mAccountManagerTestRule.getIdentityManager().getObserverCount());
    }
}
