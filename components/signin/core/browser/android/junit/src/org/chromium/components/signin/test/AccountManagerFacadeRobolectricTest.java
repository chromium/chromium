// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;
import static org.robolectric.Shadows.shadowOf;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeImpl;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.testing.local.CustomShadowUserManager;

import java.util.List;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Robolectric tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, CustomShadowUserManager.class})
public class AccountManagerFacadeRobolectricTest {
    private CustomShadowUserManager mShadowUserManager;
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    @Mock
    private UmaRecorder mUmaRecorderMock;

    @Before
    public void setUp() {
        initMocks(this);
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorderMock);
        Context context = RuntimeEnvironment.application;
        UserManager userManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        mShadowUserManager = (CustomShadowUserManager) shadowOf(userManager);
        mDelegate = new FakeAccountManagerDelegate();
        mFacade = new AccountManagerFacadeImpl(mDelegate);
    }

    private void setAccountRestrictionPatterns(String... patterns) {
        Bundle restrictions = new Bundle();
        restrictions.putStringArray(
                AccountManagerFacadeImpl.ACCOUNT_RESTRICTION_PATTERNS_KEY, patterns);
        mShadowUserManager.setApplicationRestrictions(
                RuntimeEnvironment.application.getPackageName(), restrictions);
        RuntimeEnvironment.application.sendBroadcast(
                new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    private void clearAccountRestrictionPatterns() {
        mShadowUserManager.setApplicationRestrictions(
                RuntimeEnvironment.application.getPackageName(), new Bundle());
        RuntimeEnvironment.application.sendBroadcast(
                new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    @Test
    @SmallTest
    public void testRegisterObserversCalledInConstructor() {
        FakeAccountManagerDelegate delegate = spy(new FakeAccountManagerDelegate());
        verify(delegate, never()).registerObservers();
        AccountManagerFacade accountManagerFacade = new AccountManagerFacadeImpl(delegate);
        verify(delegate).registerObservers();
    }

    @Test
    @SmallTest
    public void testCountOfAccountLoggedAfterAccountsFetched() {
        addTestAccount("test@gmail.com");
        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);
        CallbackHelper callbackHelper = new CallbackHelper();
        facade.runAfterCacheIsPopulated(() -> callbackHelper.notifyCalled());
        try {
            callbackHelper.waitForFirst();
        } catch (TimeoutException e) {
            throw new RuntimeException("Timed out waiting for callback", e);
        }
        verify(mUmaRecorderMock)
                .recordLinearHistogram("Signin.AndroidNumberOfDeviceAccounts", 1, 1, 50, 51);
    }

    @Test
    @SmallTest
    public void testCanonicalAccount() {
        addTestAccount("test@gmail.com");
        List<Account> accounts = mFacade.tryGetGoogleAccounts();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st@gmail.com"));
        Assert.assertNull(AccountUtils.findAccountByName(accounts, "te@googlemail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    @SmallTest
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");
        List<Account> accounts = mFacade.tryGetGoogleAccounts();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test.me@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st.me@gmail.com"));
    }

    @Test
    @SmallTest
    public void testGetAccounts() throws AccountManagerDelegateException {
        Assert.assertEquals(ImmutableList.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                ImmutableList.of(account, account2, account3), mFacade.getGoogleAccounts());

        removeTestAccount(account2);
        Assert.assertEquals(ImmutableList.of(account, account3), mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithAccountPattern() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@example.com");
        Account account = addTestAccount("test@example.com");
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());

        removeTestAccount(account);
        Assert.assertEquals(ImmutableList.of(account2), mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithTwoAccountPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(ImmutableList.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test1@example.com");
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        addTestAccount("test2@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsWithAccountPatternsChange() throws AccountManagerDelegateException {
        Assert.assertEquals(ImmutableList.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                ImmutableList.of(account, account2, account3), mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("test@gmail.com");
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertEquals(ImmutableList.of(account2, account3), mFacade.getGoogleAccounts());

        removeTestAccount(account3);
        Assert.assertEquals(ImmutableList.of(account2), mFacade.getGoogleAccounts());

        clearAccountRestrictionPatterns();
        Assert.assertEquals(ImmutableList.of(account, account2), mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testGetAccountsMultipleMatchingPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");
        Account account = addTestAccount("test@gmail.com"); // Matches both patterns
        Assert.assertEquals(ImmutableList.of(account), mFacade.getGoogleAccounts());
    }

    @Test
    @SmallTest
    public void testCheckChildAccount() {
        Account testAccount = addTestAccount("test@gmail.com");
        Account ucaAccount = addTestAccount(
                "uca@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_CHILD_ACCOUNT_KEY);
        Account usmAccount = addTestAccount(
                "usm@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY);
        Account bothAccount = addTestAccount("uca_usm@gmail.com",
                AccountManagerFacadeImpl.FEATURE_IS_CHILD_ACCOUNT_KEY,
                AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY);

        assertChildAccountStatus(testAccount, ChildAccountStatus.NOT_CHILD);
        assertChildAccountStatus(ucaAccount, ChildAccountStatus.REGULAR_CHILD);
        assertChildAccountStatus(usmAccount, ChildAccountStatus.USM_CHILD);
        assertChildAccountStatus(bothAccount, ChildAccountStatus.REGULAR_CHILD);
    }

    @Test(expected = IllegalStateException.class)
    @SmallTest
    public void testAccountManagerFacadeProviderGetNullInstance() {
        AccountManagerFacadeProvider.getInstance();
    }

    private Account addTestAccount(String accountEmail, String... features) {
        AccountHolder holder = AccountHolder.builder(accountEmail)
                                       .alwaysAccept(true)
                                       .addFeatures(features)
                                       .build();
        mDelegate.addAccountHolderExplicitly(holder);
        Assert.assertFalse(((AccountManagerFacadeImpl) mFacade).isUpdatePending().get());
        return holder.getAccount();
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccountHolderExplicitly(AccountHolder.builder(account).build());
    }

    private void assertChildAccountStatus(Account account, @ChildAccountStatus.Status int status) {
        final AtomicInteger callCount = new AtomicInteger();
        mFacade.checkChildAccountStatus(account, result -> {
            callCount.incrementAndGet();
            Assert.assertEquals(result, status);
        });
        Assert.assertEquals(1, callCount.get());
    }
}
