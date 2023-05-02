// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.rule.GrantPermissionRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowAccountManager;
import org.robolectric.shadows.ShadowUserManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Robolectric tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CustomShadowAsyncTask.class, ShadowUserManager.class,
                ShadowAccountManager.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountManagerFacadeImplTest {
    private static final String TEST_TOKEN_SCOPE = "test-token-scope";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final GrantPermissionRule mGrantPermissionRule =
            GrantPermissionRule.grant(Manifest.permission.GET_ACCOUNTS);

    @Mock
    ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private AccountsChangeObserver mObserverMock;

    @Mock
    private ChildAccountStatusListener mChildAccountStatusListenerMock;

    private final Context mContext = RuntimeEnvironment.application;
    private ShadowUserManager mShadowUserManager;
    private ShadowAccountManager mShadowAccountManager;
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    // Prefer to use the facade with the real system delegate instead of the fake delegate
    // to test the facade more thoroughly
    private AccountManagerFacade mFacadeWithSystemDelegate;

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mShadowUserManager =
                shadowOf((UserManager) mContext.getSystemService(Context.USER_SERVICE));
        mShadowAccountManager = shadowOf(AccountManager.get(mContext));
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mDelegate = spy(new FakeAccountManagerDelegate());
        mFacade = new AccountManagerFacadeImpl(mDelegate);

        mFacadeWithSystemDelegate =
                new AccountManagerFacadeImpl(new SystemAccountManagerDelegate());
    }

    @Test
    public void testAccountsChangerObservationInitialization() {
        mFacadeWithSystemDelegate.addObserver(mObserverMock);
        verify(mObserverMock, never()).onAccountsChanged();
        verify(mObserverMock, never()).onCoreAccountInfosChanged();

        mContext.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));

        verify(mObserverMock).onAccountsChanged();
        verify(mObserverMock).onCoreAccountInfosChanged();
    }

    @Test
    public void testCountOfAccountLoggedAfterAccountsFetched() {
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher("Signin.AndroidNumberOfDeviceAccounts", 1);
        addTestAccount("test@gmail.com");

        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);

        numberOfAccountsHistogram.assertExpected();
    }

    @Test
    public void testCanonicalAccount() {
        addTestAccount("test@gmail.com");
        List<Account> accounts = mFacade.getAccounts().getResult();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st@gmail.com"));
        Assert.assertNull(AccountUtils.findAccountByName(accounts, "te@googlemail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");
        List<Account> accounts = mFacade.getAccounts().getResult();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test.me@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st.me@gmail.com"));
    }

    @Test
    public void testGetAccounts() {
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                List.of(account, account2, account3), mFacade.getAccounts().getResult());

        removeTestAccount(account2);
        Assert.assertEquals(List.of(account, account3), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetCoreAccountInfos() {
        Account account1 = addTestAccount("test1@gmail.com");
        Account account2 = addTestAccount("test2@gmail.com");

        final List<CoreAccountInfo> coreAccountInfos = mFacade.getCoreAccountInfos().getResult();

        final CoreAccountInfo coreAccountInfo1 = CoreAccountInfo.createFromEmailAndGaiaId(
                account1.name, mFacade.getAccountGaiaId(account1.name));
        final CoreAccountInfo coreAccountInfo2 = CoreAccountInfo.createFromEmailAndGaiaId(
                account2.name, mFacade.getAccountGaiaId(account2.name));
        Assert.assertEquals(List.of(coreAccountInfo1, coreAccountInfo2), coreAccountInfos);
    }

    @Test
    public void testGetCoreAccountInfosWhenGaiaIdIsNull() {
        final String accountName = "test@gmail.com";
        AtomicBoolean accountRemoved = new AtomicBoolean(false);
        doAnswer(invocation -> {
            // Simulate removal of account during the gaia-id fetch process.
            // This method may be called after the account is already removed. Without this check
            // FakeAccountManagerDelegate.removeAccount() will crash because the account doesn't
            // exist.
            if (!accountRemoved.get()) {
                removeTestAccount(AccountUtils.createAccountFromName(invocation.getArgument(0)));
                accountRemoved.set(true);
            }
            return null;
        })
                .when(mDelegate)
                .getAccountGaiaId(accountName);

        addTestAccount(accountName);
        final List<CoreAccountInfo> coreAccountInfos = mFacade.getCoreAccountInfos().getResult();

        verify(mDelegate, atLeastOnce()).getAccountGaiaId(accountName);
        Assert.assertTrue(coreAccountInfos.isEmpty());
    }

    @Test
    public void testCoreAccountInfosAreCached() {
        final Account account = addTestAccount("test@gmail.com");

        mFacade.getCoreAccountInfos().getResult();
        mFacade.getCoreAccountInfos().getResult();

        // The second call to getCoreAccountInfos() should not re-fetch gaia id.
        verify(mDelegate).getAccountGaiaId(account.name);
    }

    @Test
    public void testGetAccountsWithAccountPattern() {
        setAccountRestrictionPatterns("*@example.com");
        Account account = addTestAccount("test@example.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        removeTestAccount(account);
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithTwoAccountPatterns() {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test1@example.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        addTestAccount("test2@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithAccountPatternsChange() {
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                List.of(account, account2, account3), mFacade.getAccounts().getResult());

        setAccountRestrictionPatterns("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertEquals(List.of(account2, account3), mFacade.getAccounts().getResult());

        removeTestAccount(account3);
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithAccountPatternsCleared() {
        final Account account1 = addTestAccount("test1@gmail.com");
        final Account account2 = addTestAccount("testexample2@example.com");
        setAccountRestrictionPatterns("*@example.com");
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());

        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), new Bundle());
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));

        Assert.assertEquals(List.of(account1, account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsMultipleMatchingPatterns() {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");
        Account account = addTestAccount("test@gmail.com"); // Matches both patterns
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());
    }

    @Test
    public void testCheckChildAccount() {
        final Account account = setFeaturesForAccount(
                "usm@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY);

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(true, account);
    }

    @Test
    public void testCheckChildAccountForAdult() {
        final Account account = setFeaturesForAccount("adult@gmail.com");

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(false, null);
    }

    @Test
    public void testGetAndInvalidateAccessToken() throws AuthException {
        final Account account = addTestAccount("test@gmail.com");
        final AccessTokenData originalToken = mFacade.getAccessToken(account, TEST_TOKEN_SCOPE);
        Assert.assertEquals("The same token should be returned before invalidating the token.",
                mFacade.getAccessToken(account, TEST_TOKEN_SCOPE).getToken(),
                originalToken.getToken());

        mFacade.invalidateAccessToken(originalToken.getToken());

        final AccessTokenData newToken = mFacade.getAccessToken(account, TEST_TOKEN_SCOPE);
        Assert.assertNotEquals(
                "A different token should be returned since the original token is invalidated.",
                newToken.getToken(), originalToken.getToken());
    }

    @Test(expected = IllegalStateException.class)
    public void testAccountManagerFacadeProviderGetNullInstance() {
        AccountManagerFacadeProvider.getInstance();
    }

    @Test
    public void testGetAccountCapabilitiesResponseYes() {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final AccountHolder accountHolder = AccountHolder.createFromEmail("test1@gmail.com");
        mDelegate.addAccount(accountHolder);

        doReturn(CapabilityResponse.YES)
                .when(mDelegate)
                .hasCapability(eq(accountHolder.getAccount()), any());

        AccountCapabilities capabilities =
                facade.getAccountCapabilities(accountHolder.getAccount()).getResult();
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.TRUE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.TRUE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.TRUE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.TRUE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseNo() {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final AccountHolder accountHolder = AccountHolder.createFromEmail("test1@gmail.com");
        mDelegate.addAccount(accountHolder);

        doReturn(CapabilityResponse.NO)
                .when(mDelegate)
                .hasCapability(eq(accountHolder.getAccount()), any());

        AccountCapabilities capabilities =
                facade.getAccountCapabilities(accountHolder.getAccount()).getResult();
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.FALSE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseException() {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final AccountHolder accountHolder = AccountHolder.createFromEmail("test1@gmail.com");
        mDelegate.addAccount(accountHolder);

        doReturn(CapabilityResponse.EXCEPTION)
                .when(mDelegate)
                .hasCapability(eq(accountHolder.getAccount()), any());

        AccountCapabilities capabilities =
                facade.getAccountCapabilities(accountHolder.getAccount()).getResult();
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.UNKNOWN);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.UNKNOWN);
    }

    private Account setFeaturesForAccount(String email, String... features) {
        final Account account = AccountUtils.createAccountFromName(email);
        mShadowAccountManager.setFeatures(account, features);
        return account;
    }

    private void setAccountRestrictionPatterns(String... patterns) {
        Bundle restrictions = new Bundle();
        restrictions.putStringArray("RestrictAccountsToPatterns", patterns);
        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), restrictions);
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    private Account addTestAccount(String accountEmail) {
        final AccountHolder holder = AccountHolder.createFromEmail(accountEmail);
        mDelegate.addAccount(holder);
        return holder.getAccount();
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccount(AccountHolder.createFromAccount(account));
    }
}
