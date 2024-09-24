// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
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
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
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
    private static final AccountInfo TEST_ACCOUNT =
            new AccountInfo.Builder("test@gmail.com", "testGaiaId").build();

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
    private AccountManagerFacadeImpl mFacade;

    @Mock private AccountManagerDelegate mDelegateMock;

    // Prefer to use the facade with the real system delegate instead of the fake delegate
    // to test the facade more thoroughly
    private AccountManagerFacade mFacadeWithSystemDelegate;

    @Before
    public void setUp() {
        lenient().when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mShadowUserManager =
                shadowOf((UserManager) mContext.getSystemService(Context.USER_SERVICE));
        mShadowAccountManager = shadowOf(AccountManager.get(mContext));
        mPostTaskRunner = new ShadowPostTaskImpl();
        ShadowPostTask.setTestImpl(mPostTaskRunner);
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
        mDelegate = spy(new FakeAccountManagerDelegate());
        mFacade = new AccountManagerFacadeImpl(mDelegate);
        mFacade.resetAccountsForTesting();

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
    public void testAccountFetching() throws Exception {
        HistogramWatcher retriesHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.GetAccountsBackoffRetries")
                        .build();
        HistogramWatcher successHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.GetAccountsBackoffSuccess")
                        .build();

        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        delegate.addAccount(TEST_ACCOUNT);
        AccountManagerFacade facade = new AccountManagerFacadeImpl(delegate);

        assertEquals(facade.getCoreAccountInfos().getResult(), List.of(TEST_ACCOUNT));
        assertTrue(facade.didAccountFetchSucceed());
        retriesHistogram.assertExpected();
        successHistogram.assertExpected();
    }

    @Test
    public void testErrorFetchingAccounts() throws Exception {
        doThrow(AccountManagerDelegateException.class)
                .doReturn(new Account[] {CoreAccountInfo.getAndroidAccountFrom(TEST_ACCOUNT)})
                .when(mDelegateMock)
                .getAccountsSynchronous();
        doReturn(TEST_ACCOUNT.getGaiaId())
                .when(mDelegateMock)
                .getAccountGaiaId(TEST_ACCOUNT.getEmail());

        HistogramWatcher retriesHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Signin.GetAccountsBackoffRetries", /* retries= */ 1)
                        .build();
        HistogramWatcher successHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Signin.GetAccountsBackoffSuccess", true)
                        .build();

        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegateMock);

        // Called once on AccountManagerFacade creation.
        verify(mDelegateMock).getAccountsSynchronous();
        assertFalse(facade.getCoreAccountInfos().isFulfilled());

        // The delegate call is retried once, and succeeds.
        mPostTaskRunner.runAll();
        verify(mDelegateMock, times(2)).getAccountsSynchronous();
        assertTrue(facade.getCoreAccountInfos().isFulfilled());
        assertTrue(facade.didAccountFetchSucceed());
        assertEquals(facade.getCoreAccountInfos().getResult(), List.of(TEST_ACCOUNT));
        retriesHistogram.assertExpected();
        successHistogram.assertExpected();
    }

    @Test
    public void testErrorFetchingAccounts_maxNumberOfRetries() throws Exception {
        doThrow(AccountManagerDelegateException.class).when(mDelegate).getAccountsSynchronous();
        HistogramWatcher retriesHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Signin.GetAccountsBackoffRetries")
                        .build();
        HistogramWatcher successHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Signin.GetAccountsBackoffSuccess", false)
                        .build();

        mDelegate.callOnCoreAccountInfoChanged();
        // Called once on AccountManagerFacade creation and a second time when
        // onCoreAccountInfoChanged is called.
        verify(mDelegate, times(2)).getAccountsSynchronous();

        // The delegate call fails indefinitely but is only retried MAXIMUM_RETRIES times (plus the
        // two interactions checked above).
        mPostTaskRunner.runAll();
        verify(mDelegate, times(AccountManagerFacadeImpl.MAXIMUM_RETRIES + 2))
                .getAccountsSynchronous();
        assertFalse(mFacade.didAccountFetchSucceed());
        assertEquals(mFacade.getCoreAccountInfos().getResult(), List.of());
        retriesHistogram.assertExpected();
        successHistogram.assertExpected();
    }

    @Test
    public void testAccountFetchingFailsThenSucceeds() throws Exception {
        // Initially, account fetching fails.
        doThrow(AccountManagerDelegateException.class).when(mDelegate).getAccountsSynchronous();
        mDelegate.callOnCoreAccountInfoChanged();
        mPostTaskRunner.runAll();
        assertFalse(mFacade.didAccountFetchSucceed());
        assertEquals(mFacade.getCoreAccountInfos().getResult(), List.of());

        // Accounts are updated again.
        mDelegate.callOnCoreAccountInfoChanged();
        // Account fetch is still marked as non-successful.
        assertFalse(mFacade.didAccountFetchSucceed());
        // This time account fetch will succeed.
        doReturn(new Account[] {CoreAccountInfo.getAndroidAccountFrom(TEST_ACCOUNT)})
                .when(mDelegate)
                .getAccountsSynchronous();
        doReturn(TEST_ACCOUNT.getGaiaId())
                .when(mDelegate)
                .getAccountGaiaId(TEST_ACCOUNT.getEmail());
        mPostTaskRunner.runAll();
        assertTrue(mFacade.didAccountFetchSucceed());
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

        removeTestAccount(accountInfo1.getId());
        Assert.assertEquals(List.of(accountInfo2), mFacade.getCoreAccountInfos().getResult());
    }

    @Test
    public void testGetCoreAccountInfosWhenGaiaIdIsNull() throws Exception {
        final String accountEmail = "test@gmail.com";
        final String accountGaiaId = FakeAccountManagerDelegate.toGaiaId(accountEmail);
        AtomicBoolean accountRemoved = new AtomicBoolean(false);
        doAnswer(
                        invocation -> {
                            // Simulate removal of account during the gaia-id fetch process.
                            // This method may be called after the account is already removed.
                            // Without this check FakeAccountManagerDelegate.removeAccount() will
                            // crash because the account doesn't exist.
                            if (!accountRemoved.get()) {
                                removeTestAccount(new CoreAccountId(accountGaiaId));
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

        removeTestAccount(accountInfo1.getId());
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
        mDelegate.callOnCoreAccountInfoChanged();
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

        removeTestAccount(accountInfo3.getId());
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
        final CoreAccountInfo coreAccountInfo =
                setFeaturesForAccount(
                        "usm@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY);

        mFacadeWithSystemDelegate.checkChildAccountStatus(
                coreAccountInfo, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(true, coreAccountInfo);
    }

    @Test
    public void testCheckChildAccountForAdult() {
        final CoreAccountInfo coreAccountInfo = setFeaturesForAccount("adult@gmail.com");

        mFacadeWithSystemDelegate.checkChildAccountStatus(
                coreAccountInfo, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(false, null);
    }

    @Test
    public void testGetAccountCapabilitiesResponseYes() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        CoreAccountInfo accountInfo = addTestAccount("test@gmail.com");

        doReturn(CapabilityResponse.YES)
                .when(mDelegate)
                .hasCapability(eq(CoreAccountInfo.getAndroidAccountFrom(accountInfo)), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(accountInfo).getResult();
        Assert.assertEquals(capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(), Tribool.TRUE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.TRUE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.TRUE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.TRUE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseNo() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        CoreAccountInfo accountInfo = addTestAccount("test@gmail.com");

        doReturn(CapabilityResponse.NO)
                .when(mDelegate)
                .hasCapability(eq(CoreAccountInfo.getAndroidAccountFrom(accountInfo)), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(accountInfo).getResult();
        Assert.assertEquals(capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(), Tribool.FALSE);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.FALSE);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.FALSE);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.FALSE);
    }

    @Test
    public void testGetAccountCapabilitiesResponseException() throws Exception {
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        CoreAccountInfo accountInfo = addTestAccount("test@gmail.com");

        doReturn(CapabilityResponse.EXCEPTION)
                .when(mDelegate)
                .hasCapability(eq(CoreAccountInfo.getAndroidAccountFrom(accountInfo)), any());

        AccountCapabilities capabilities = facade.getAccountCapabilities(accountInfo).getResult();
        Assert.assertEquals(capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.isSubjectToParentalControls(), Tribool.UNKNOWN);
        Assert.assertEquals(capabilities.canRunChromePrivacySandboxTrials(), Tribool.UNKNOWN);
        Assert.assertEquals(
                capabilities.isSubjectToChromePrivacySandboxRestrictedMeasurementNotice(),
                Tribool.UNKNOWN);
    }

    private CoreAccountInfo setFeaturesForAccount(String email, String... features) {
        final Account account = AccountUtils.createAccountFromName(email);
        final CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId(email, "notUsedGaiaId");
        mShadowAccountManager.setFeatures(account, features);
        return coreAccountInfo;
    }

    private void setAccountRestrictionPatterns(String... patterns) {
        Bundle restrictions = new Bundle();
        restrictions.putStringArray("RestrictAccountsToPatterns", patterns);
        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), restrictions);
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    private CoreAccountInfo addTestAccount(String accountEmail) {
        AccountInfo accountInfo =
                new AccountInfo.Builder(
                                accountEmail, FakeAccountManagerDelegate.toGaiaId(accountEmail))
                        .build();
        mDelegate.addAccount(accountInfo);
        return accountInfo;
    }

    private void removeTestAccount(CoreAccountId accountId) {
        mDelegate.removeAccount(accountId);
    }
}
