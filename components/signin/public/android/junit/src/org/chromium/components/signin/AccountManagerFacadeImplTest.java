// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
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
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Robolectric tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            CustomShadowAsyncTask.class,
            ShadowUserManager.class,
            ShadowAccountManager.class,
            ShadowPostTask.class
        })
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountManagerFacadeImplTest {
    private static final String TEST_TOKEN_SCOPE = "test-token-scope";

    private static class ShadowPostTaskImpl implements ShadowPostTask.TestImpl {
        private final List<Runnable> mRunnables = new ArrayList<>();

        @Override
        public void postDelayedTask(@TaskTraits int traits, Runnable task, long delay) {
            mRunnables.add(task);
        }

        void runAll() {
            for (int index = 0; index < mRunnables.size(); index++) {
                mRunnables.get(index).run();
            }
            mRunnables.clear();
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final GrantPermissionRule mGrantPermissionRule =
            GrantPermissionRule.grant(Manifest.permission.GET_ACCOUNTS);

    @Mock ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock private AccountsChangeObserver mObserverMock;

    @Mock private ChildAccountStatusListener mChildAccountStatusListenerMock;

    private final Context mContext = RuntimeEnvironment.application;
    private ShadowUserManager mShadowUserManager;
    private ShadowAccountManager mShadowAccountManager;
    private ShadowPostTaskImpl mPostTaskRunner;
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
        mPostTaskRunner = new ShadowPostTaskImpl();
        ShadowPostTask.setTestImpl(mPostTaskRunner);
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mDelegate = spy(new FakeAccountManagerDelegate());
        mFacade = new AccountManagerFacadeImpl(mDelegate);

        mFacadeWithSystemDelegate =
                new AccountManagerFacadeImpl(new SystemAccountManagerDelegate());
    }

    @Test
    public void testAccountsChangerObservationInitialization() {
        mFacadeWithSystemDelegate.addObserver(mObserverMock);
        verify(mObserverMock, never()).onCoreAccountInfosChanged();

        mContext.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));

        verify(mObserverMock).onCoreAccountInfosChanged();
    }

    @Test
    public void testCountOfAccountLoggedAfterAccountsFetched() throws Exception {
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher("Signin.AndroidNumberOfDeviceAccounts", 1);
        addTestAccount("test@gmail.com");

        new AccountManagerFacadeImpl(mDelegate);

        numberOfAccountsHistogram.assertExpected();
    }

    @Test
    public void testCanonicalAccount() throws Exception {
        addTestAccount("test@gmail.com");
        List<CoreAccountInfo> coreAccountInfos = mFacade.getCoreAccountInfos().getResult();

        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "test@gmail.com"));
        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "Test@gmail.com"));
        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "te.st@gmail.com"));
        Assert.assertNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "te@googlemail.com"));
    }

    @Test
    public void testErrorFetchingAccounts() throws Exception {
        AccountHolder accountHolder = AccountHolder.createFromEmail("test@gmail.com");
        doThrow(AccountManagerDelegateException.class)
                .doReturn(new Account[] {accountHolder.getAccount()})
                .when(mDelegate)
                .getAccountsSynchronous();

        mDelegate.callOnCoreAccountInfoChanged();
        // Called once on AccountManagerFacade creation and a second time when the account is added.
        // TODO(crbug.com/1502123): Add verification that getCoreAccountInfos isn't fulfilled until
        // getAccountsSynchronous stops throwing exceptions (and that it is correctly fulfilled when
        // it stops throwing).
        verify(mDelegate, times(2)).getAccountsSynchronous();

        // The delegate call is retried once, and succeeds (for a total of three interactions with
        // the mock).
        mPostTaskRunner.runAll();
        verify(mDelegate, times(3)).getAccountsSynchronous();
    }

    @Test
    public void testErrorFetchingAccounts_maxNumberOfRetries() throws Exception {
        doThrow(AccountManagerDelegateException.class).when(mDelegate).getAccountsSynchronous();

        mDelegate.callOnCoreAccountInfoChanged();
        // Called once on AccountManagerFacade creation and a second time when the account is added.
        verify(mDelegate, times(2)).getAccountsSynchronous();

        // The delegate call fails indefinitely but is only retried MAXIMUM_RETRIES times (plus the
        // two interactions checked above).
        mPostTaskRunner.runAll();
        verify(mDelegate, times(AccountManagerFacadeImpl.MAXIMUM_RETRIES + 2))
                .getAccountsSynchronous();
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    public void testNonCanonicalAccount() throws Exception {
        addTestAccount("test.me@gmail.com");
        List<CoreAccountInfo> coreAccountInfos = mFacade.getCoreAccountInfos().getResult();

        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "test.me@gmail.com"));
        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "testme@gmail.com"));
        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "Testme@gmail.com"));
        Assert.assertNotNull(
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, "te.st.me@gmail.com"));
    }

    @Test
    public void testGetCoreAccountInfos() throws Exception {
        CoreAccountInfo accountInfo1 = addTestAccount("test1@gmail.com");
        CoreAccountInfo accountInfo2 = addTestAccount("test2@gmail.com");

        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());

        removeTestAccount(accountInfo1.getEmail());
        Assert.assertEquals(List.of(accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosWhenGaiaIdIsNull() throws Exception {
        final String accountEmail = "test@gmail.com";
        AtomicBoolean accountRemoved = new AtomicBoolean(false);
        doAnswer(
                        invocation -> {
                            // Simulate removal of account during the gaia-id fetch process.
                            // This method may be called after the account is already removed.
                            // Without this check FakeAccountManagerDelegate.removeAccount() will
                            // crash because the account doesn't exist.
                            if (!accountRemoved.get()) {
                                removeTestAccount(accountEmail);
                                accountRemoved.set(true);
                            }
                            return null;
                        })
                .when(mDelegate)
                .getAccountGaiaId(accountEmail);

        addTestAccount(accountEmail);
        final List<CoreAccountInfo> coreAccountInfos = mFacade.getCoreAccountInfos().getResult();

        verify(mDelegate, atLeastOnce()).getAccountGaiaId(accountEmail);
        Assert.assertTrue(coreAccountInfos.isEmpty());
    }

    @Test
    public void testCoreAccountInfosAreCached() throws Exception {
        final String accountEmail = "test@gmail.com";
        addTestAccount(accountEmail);

        mFacade.getCoreAccountInfos().getResult();
        mFacade.getCoreAccountInfos().getResult();

        // The second call to getCoreAccountInfos() should not re-fetch gaia id.
        verify(mDelegate).getAccountGaiaId(accountEmail);
    }

    @Test
    public void testGetCoreAccountInfosWithAccountPattern() throws Exception {
        setAccountRestrictionPatterns("*@example.com");
        CoreAccountInfo accountInfo1 = addTestAccount("test1@example.com");
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo2 = addTestAccount("test2@example.com");
        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());

        removeTestAccount(accountInfo1.getEmail());
        Assert.assertEquals(List.of(accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosWithTwoAccountPatterns() throws Exception {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo1 = addTestAccount("test1@example.com");
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosWithAccountPatternsChange() throws Exception {
        Assert.assertEquals(List.of(), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo1 = addTestAccount("test1@gmail.com");
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo2 = addTestAccount("test2@example.com");
        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());

        CoreAccountInfo accountInfo3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2, accountInfo3),
                mFacade.getCoreAccountInfos().getResult());

        setAccountRestrictionPatterns("test1@gmail.com");
        Assert.assertEquals(List.of(accountInfo1), mFacade.getCoreAccountInfos().getResult());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertEquals(
                List.of(accountInfo2, accountInfo3), mFacade.getCoreAccountInfos().getResult());

        removeTestAccount(accountInfo3.getEmail());
        Assert.assertEquals(List.of(accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosWithAccountPatternsCleared() throws Exception {
        CoreAccountInfo accountInfo1 = addTestAccount("test1@gmail.com");
        CoreAccountInfo accountInfo2 = addTestAccount("test2@example.com");
        setAccountRestrictionPatterns("*@example.com");
        Assert.assertEquals(List.of(accountInfo2), mFacade.getCoreAccountInfos().getResult());

        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), new Bundle());
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));

        Assert.assertEquals(
                List.of(accountInfo1, accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosMultipleMatchingPatterns() throws Exception {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");

        // Matches both patterns
        CoreAccountInfo accountInfo = addTestAccount("test@gmail.com");

        Assert.assertEquals(List.of(accountInfo), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testCheckChildAccount() {
        final Account account =
                setFeaturesForAccount(
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
    public void testGetAndInvalidateAccessToken() throws Exception {
        final CoreAccountInfo coreAccountInfo = addTestAccount("test@gmail.com");
        final AccessTokenData originalToken =
                mFacade.getAccessToken(coreAccountInfo, TEST_TOKEN_SCOPE);
        Assert.assertEquals(
                "The same token should be returned before invalidating the token.",
                mFacade.getAccessToken(coreAccountInfo, TEST_TOKEN_SCOPE).getToken(),
                originalToken.getToken());

        mFacade.invalidateAccessToken(originalToken.getToken());

        final AccessTokenData newToken = mFacade.getAccessToken(coreAccountInfo, TEST_TOKEN_SCOPE);
        Assert.assertNotEquals(
                "A different token should be returned since the original token is invalidated.",
                newToken.getToken(),
                originalToken.getToken());
    }

    @Test(expected = IllegalStateException.class)
    public void testAccountManagerFacadeProviderGetNullInstance() {
        AccountManagerFacadeProvider.getInstance();
    }

    @Test
    public void testGetAccountCapabilitiesResponseYes() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final Account account = AccountUtils.createAccountFromName("test1@gmail.com");
        mDelegate.addAccount(AccountHolder.createFromAccount(account));

        doReturn(CapabilityResponse.YES).when(mDelegate).hasCapability(eq(account), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(account).getResult();
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.TRUE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.TRUE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.TRUE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.TRUE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseNo() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final Account account = AccountUtils.createAccountFromName("test1@gmail.com");
        mDelegate.addAccount(AccountHolder.createFromAccount(account));

        doReturn(CapabilityResponse.NO).when(mDelegate).hasCapability(eq(account), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(account).getResult();
        Assert.assertEquals(capabilities.canOfferExtendedSyncPromos(), Tribool.FALSE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.FALSE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseException() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        final Account account = AccountUtils.createAccountFromName("test1@gmail.com");
        mDelegate.addAccount(AccountHolder.createFromAccount(account));

        doReturn(CapabilityResponse.EXCEPTION).when(mDelegate).hasCapability(eq(account), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(account).getResult();
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

    private CoreAccountInfo addTestAccount(String accountEmail) {
        mDelegate.addAccount(AccountHolder.createFromEmail(accountEmail));
        return AccountUtils.findCoreAccountInfoByEmail(
                mFacade.getCoreAccountInfos().getResult(), accountEmail);
    }

    private void removeTestAccount(String accountEmail) {
        mDelegate.removeAccount(AccountHolder.createFromEmail(accountEmail));
    }
}
