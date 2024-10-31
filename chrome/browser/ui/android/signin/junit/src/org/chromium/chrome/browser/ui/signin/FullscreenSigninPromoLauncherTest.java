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
import android.content.Intent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
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

    @Mock private SigninAndHistorySyncActivityLauncher mFullscreenSigninLauncherMock;

    @Mock private Profile mProfile;

    @Mock private Context mContext;

    @Mock private Intent mSigninIntent;

    private final SigninPreferencesManager mPrefManager = SigninPreferencesManager.getInstance();

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
    public void whenAccountCacheNotPopulated() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(38);
        mFakeAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        Assert.assertEquals(38, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
    }

    @Test
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        Assert.assertEquals(42, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
    }

    @EnableFeatures(ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO)
    @Test
    public void promoVisibleWhenForcingSigninPromoAtStartup() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mFullscreenSigninLauncherMock.createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO)))
                .thenReturn(mSigninIntent);

        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));

        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO)
    public void promoNotShownWhenForcingSigninPromoAtStartupOnAuto() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO));
    }

    @Test
    public void whenSignedInAndSyncingShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC))
                .thenReturn(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
    }

    @Test
    public void manuallySignedOutReturnsFalse() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mPrefServiceMock.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))
                .thenReturn(TestAccounts.ACCOUNT1.getEmail());
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(41);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
    }

    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        when(mFullscreenSigninLauncherMock.createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO)))
                .thenReturn(mSigninIntent);

        // Old implementation hasn't been storing account list
        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));

        verify(mContext).startActivity(mSigninIntent);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {TestAccounts.ACCOUNT1.getEmail()});
    }

    @Test
    public void whenNoAccountListStoredOnAutoShouldReturnFalse() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        TestAccounts.ACCOUNT1.getEmail()))
                .thenReturn(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(40);

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO));
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(null, mPrefManager.getSigninPromoLastAccountEmails());
    }

    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(Set.of(TestAccounts.ACCOUNT1.getEmail()));
        when(mFullscreenSigninLauncherMock.createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO)))
                .thenReturn(mSigninIntent);

        Assert.assertTrue(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));

        verify(mContext).startActivity(mSigninIntent);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    public void whenHasNewAccountOnAutoShouldReturnFalse() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(
                        AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(Set.of(TestAccounts.ACCOUNT1.getEmail()));

        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.SIGNIN_PROMO));
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                new String[] {TestAccounts.ACCOUNT1.getEmail()},
                mPrefManager.getSigninPromoLastAccountEmails().toArray());
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(Set.of(TestAccounts.ACCOUNT1.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {TestAccounts.ACCOUNT1.getEmail()});
    }

    @Test
    public void whenNoNewAccountsShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(TestAccounts.ACCOUNT1.getEmail(), TestAccounts.ACCOUNT2.getEmail()));
        Assert.assertFalse(
                FullscreenSigninPromoLauncher.launchPromoIfNeeded(
                        mContext, mProfile, mFullscreenSigninLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mFullscreenSigninLauncherMock, never())
                .createFullscreenSigninIntent(any(), any(), any(), anyInt());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }
}
