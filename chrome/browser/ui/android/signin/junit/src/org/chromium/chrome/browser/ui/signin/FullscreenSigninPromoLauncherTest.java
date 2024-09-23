// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Set;

/** Tests for {@link FullscreenSigninPromoLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS,
    ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO
})
public class FullscreenSigninPromoLauncherTest {
    private static final int CURRENT_MAJOR_VERSION = 42;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final JniMocker mocker = new JniMocker();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Mock private UserPrefs.Natives mUserPrefsNativeMock;

    @Mock private PrefService mPrefServiceMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private SyncConsentActivityLauncher mSyncPromoLauncherMock;

    @Mock private SigninAndHistorySyncActivityLauncher mUpgradePromoLauncherMock;

    @Mock private Profile mProfile;

    private final Context mContext = RuntimeEnvironment.systemContext;
    private final SigninPreferencesManager mPrefManager = SigninPreferencesManager.getInstance();
    private final AccountCapabilitiesBuilder mAccountCapabilitiesBuilder =
            new AccountCapabilitiesBuilder();

    @Before
    public void setUp() {
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativeMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(mProfile))
                .thenReturn(mIdentityManagerMock);
        when(mUserPrefsNativeMock.get(mProfile)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME)).thenReturn("");
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
    }

    @After
    public void tearDown() {
        mPrefManager.clearSigninPromoLastShownPrefsForTesting();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void whenAccountCacheNotPopulated() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(38);
        mFakeAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        Assert.assertEquals(38, mPrefManager.getSigninPromoLastShownVersion());
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        Assert.assertEquals(42, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
    }

    @EnableFeatures({ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO})
    @DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @Test
    public void promoVisibleWhenForcingSigninPromoAtStartup() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock)
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @EnableFeatures({
        ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    @Test
    public void promoVisibleWhenForcingSigninPromoAtStartup_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void
            promoNotShownWhenForcingSigninPromoAtStartupOnAuto_replaceSyncWithSigninPromosEnabled() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(mContext, mProfile);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void whenSignedInAndSyncingShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void manuallySignedOutReturnsFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        when(mPrefServiceMock.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(41);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void whenNoAccountsShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mPrefManager.setSigninPromoLastShownVersion(40);
        // Old implementation hasn't been storing account list
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock)
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()});
    }

    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void whenNoAccountListStoredShouldReturnTrue_replaceSyncWithSigninPromosEnabled() {
        final AccountInfo accountInfo =
                new AccountInfo.Builder(AccountManagerTestRule.TEST_ACCOUNT_1)
                        .accountCapabilities(
                                mAccountCapabilitiesBuilder
                                        .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(
                                                true)
                                        .build())
                        .build();
        mAccountManagerTestRule.addAccount(accountInfo);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(accountInfo.getEmail()))
                .thenReturn(accountInfo);
        mPrefManager.setSigninPromoLastShownVersion(40);
        // Old implementation hasn't been storing account list
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {accountInfo.getEmail()});
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            whenNoAccountListStoredOnAutoShouldReturnFalse_replaceSyncWithSigninPromosEnabled() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        final AccountInfo accountInfo =
                new AccountInfo.Builder(AccountManagerTestRule.TEST_ACCOUNT_1)
                        .accountCapabilities(
                                mAccountCapabilitiesBuilder
                                        .setCanShowHistorySyncOptInsWithoutMinorModeRestrictions(
                                                true)
                                        .build())
                        .build();
        mAccountManagerTestRule.addAccount(accountInfo);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(accountInfo.getEmail()))
                .thenReturn(accountInfo);
        mPrefManager.setSigninPromoLastShownVersion(40);

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(mContext, mProfile);
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(null, mPrefManager.getSigninPromoLastAccountEmails());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenCapabilityIsNotAvailable() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never())
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenCapabilityIsNotAvailable_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never())
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
    }

    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock)
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void whenHasNewAccountShouldReturnTrue_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));

        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenHasNewAccountOnAutoShouldReturnFalse_replaceSyncWithSigninPromosEnabled() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(mContext, mProfile);
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                new String[] {AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()},
                mPrefManager.getSigninPromoLastAccountEmails().toArray());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenAccountListUnchangedShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()});
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenAccountListUnchangedShouldReturnFalse_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {AccountManagerTestRule.TEST_ACCOUNT_1.getEmail()});
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenNoNewAccountsShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(
                        AccountManagerTestRule.TEST_ACCOUNT_1.getEmail(),
                        AccountManagerTestRule.TEST_ACCOUNT_2.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void whenNoNewAccountsShouldReturnFalse_replaceSyncWithSigninPromos() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(
                        AccountManagerTestRule.TEST_ACCOUNT_1.getEmail(),
                        AccountManagerTestRule.TEST_ACCOUNT_2.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            promoHiddenWhenDefaultAccountCanNotShowHistorySyncOptInsWithoutMinorModeRestrictions() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);
        mPrefManager.setSigninPromoLastShownVersion(38);

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));

        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            promoVisibleWhenDefaultAccountCanNotShowHistorySyncOptInsWithoutMinorModeRestrictions_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_2);
        mPrefManager.setSigninPromoLastShownVersion(38);

        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));

        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
    }

    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void
            promoVisibleWhenTheSecondaryAccountCanNotShowHistorySyncOptInsWithoutMinorModeRestrictions() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        eq(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        eq(AccountManagerTestRule.AADC_MINOR_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));

        verify(mSyncPromoLauncherMock)
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        verify(mUpgradePromoLauncherMock, never())
                .launchUpgradePromoActivityIfAllowed(any(), any());
    }

    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @Test
    public void
            promoVisibleWhenTheSecondaryAccountCanNotShowHistorySyncOptInsWithoutMinorModeRestrictions_replaceSyncWithSigninPromosEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        eq(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        eq(AccountManagerTestRule.AADC_MINOR_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext,
                        mProfile,
                        mSyncPromoLauncherMock,
                        mUpgradePromoLauncherMock,
                        CURRENT_MAJOR_VERSION));

        verify(mSyncPromoLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        verify(mUpgradePromoLauncherMock).launchUpgradePromoActivityIfAllowed(mContext, mProfile);
    }
}
