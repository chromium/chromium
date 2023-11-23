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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Set;

/** Tests for {@link FullScreenSyncPromoUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS,
    ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO
})
public class FullScreenSyncPromoTest {
    private static final int CURRENT_MAJOR_VERSION = 42;
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock private UserPrefs.Natives mUserPrefsNativeMock;

    @Mock private PrefService mPrefServiceMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private SyncConsentActivityLauncher mLauncherMock;

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
    }

    @After
    public void tearDown() {
        mPrefManager.clearSigninPromoLastShownPrefsForTesting();
    }

    @Test
    public void whenAccountCacheNotPopulated() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(38);
        mFakeAccountManagerFacade.blockGetCoreAccountInfos();
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        Assert.assertEquals(38, mPrefManager.getSigninPromoLastShownVersion());
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        Assert.assertEquals(42, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
    }

    @EnableFeatures({ChromeFeatureList.FORCE_STARTUP_SIGNIN_PROMO})
    @Test
    public void promoVisibleWhenForcingSigninPromoAtStartup() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        Assert.assertTrue(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mLauncherMock).launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
    }

    @Test
    public void whenSignedInShouldReturnFalse() {
        final CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC))
                .thenReturn(coreAccountInfo);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void manuallySignedOutReturnsFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        when(mPrefServiceMock.getString(Pref.GOOGLE_SERVICES_LAST_SYNCING_USERNAME))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(41);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade, never()).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        final AccountInfo accountInfo =
                mAccountManagerTestRule.addAccount(
                        "test@gmail.com",
                        mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(true).build());
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(accountInfo.getEmail()))
                .thenReturn(accountInfo);
        mPrefManager.setSigninPromoLastShownVersion(40);
        // Old implementation hasn't been storing account list
        Assert.assertTrue(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mLauncherMock).launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {accountInfo.getEmail()});
    }

    @Test
    public void whenCapabilityIsNotAvailable() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(40);
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mLauncherMock, never())
                .launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
    }

    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        final AccountInfo account1 =
                mAccountManagerTestRule.addAccount(
                        "test1@gmail.com",
                        mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(true).build());
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(account1.getEmail()))
                .thenReturn(account1);
        mAccountManagerTestRule.addAccount("test2@gmail.com");
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL));
        Assert.assertTrue(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mLauncherMock).launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
        Assert.assertEquals(CURRENT_MAJOR_VERSION, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL));
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertArrayEquals(
                mPrefManager.getSigninPromoLastAccountEmails().toArray(),
                new String[] {AccountManagerTestRule.TEST_ACCOUNT_EMAIL});
    }

    @Test
    public void whenNoNewAccountsShouldReturnFalse() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountEmails(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL, "test2@gmail.com"));
        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));
        verify(mFakeAccountManagerFacade).getCoreAccountInfos();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
        Assert.assertEquals(40, mPrefManager.getSigninPromoLastShownVersion());
        Assert.assertEquals(2, mPrefManager.getSigninPromoLastAccountEmails().size());
    }

    @Test
    public void promoHiddenWhenDefaultAccountCanNotOfferExtendedSyncPromos() {
        mAccountManagerTestRule.addAccount(
                "test1@gmail.com",
                mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(false).build());
        mAccountManagerTestRule.addAccount("test2@gmail.com");
        mPrefManager.setSigninPromoLastShownVersion(38);

        Assert.assertFalse(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));

        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void promoVisibleWhenTheSecondaryAccountCanNotOfferExtendedSyncPromos() {
        final AccountInfo account1 =
                mAccountManagerTestRule.addAccount(
                        "test1@gmail.com",
                        mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(true).build());
        final AccountInfo account2 =
                mAccountManagerTestRule.addAccount(
                        "test2@gmail.com",
                        mAccountCapabilitiesBuilder.setCanOfferExtendedSyncPromos(false).build());
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(eq(account1.getEmail())))
                .thenReturn(account1);
        when(mIdentityManagerMock.findExtendedAccountInfoByEmailAddress(eq(account2.getEmail())))
                .thenReturn(account2);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertTrue(
                FullScreenSyncPromoUtil.launchPromoIfNeeded(
                        mContext, mProfile, mLauncherMock, CURRENT_MAJOR_VERSION));

        verify(mLauncherMock).launchActivityIfAllowed(mContext, SigninAccessPoint.SIGNIN_PROMO);
    }
}
