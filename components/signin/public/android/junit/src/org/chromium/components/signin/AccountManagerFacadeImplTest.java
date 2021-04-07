// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.testing.local.CustomShadowUserManager;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Robolectric tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, CustomShadowUserManager.class})
public class AccountManagerFacadeImplTest {
    private static final String TEST_TOKEN_SCOPE = "test-token-scope";

    private CustomShadowUserManager mShadowUserManager;
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private UmaRecorder mUmaRecorderMock;

    @Before
    public void setUp() {
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
    public void testRegisterObserversCalledInConstructor() {
        FakeAccountManagerDelegate delegate = spy(new FakeAccountManagerDelegate());
        verify(delegate, never()).registerObservers();
        AccountManagerFacade accountManagerFacade = new AccountManagerFacadeImpl(delegate);
        verify(delegate).registerObservers();
    }

    @Test
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
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");
        List<Account> accounts = mFacade.tryGetGoogleAccounts();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test.me@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st.me@gmail.com"));
    }

    @Test
    public void testGetAccounts() throws AccountManagerDelegateException {
        Assert.assertEquals(List.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(List.of(account, account2, account3), mFacade.getGoogleAccounts());

        removeTestAccount(account2);
        Assert.assertEquals(List.of(account, account3), mFacade.getGoogleAccounts());
    }

    @Test
    public void testGetAccountsWithAccountPattern() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@example.com");
        Account account = addTestAccount("test@example.com");
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());

        removeTestAccount(account);
        Assert.assertEquals(List.of(account2), mFacade.getGoogleAccounts());
    }

    @Test
    public void testGetAccountsWithTwoAccountPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test1@example.com");
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        addTestAccount("test2@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());
    }

    @Test
    public void testGetAccountsWithAccountPatternsChange() throws AccountManagerDelegateException {
        Assert.assertEquals(List.of(), mFacade.getGoogleAccounts());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(List.of(account, account2, account3), mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertEquals(List.of(account2, account3), mFacade.getGoogleAccounts());

        removeTestAccount(account3);
        Assert.assertEquals(List.of(account2), mFacade.getGoogleAccounts());

        clearAccountRestrictionPatterns();
        Assert.assertEquals(List.of(account, account2), mFacade.getGoogleAccounts());
    }

    @Test
    public void testGetAccountsMultipleMatchingPatterns() throws AccountManagerDelegateException {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");
        Account account = addTestAccount("test@gmail.com"); // Matches both patterns
        Assert.assertEquals(List.of(account), mFacade.getGoogleAccounts());
    }

    @Test
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

    private Account addTestAccount(String accountEmail, String... features) {
        AccountHolder holder = AccountHolder.builder(accountEmail)
                                       .addFeatures(features)
                                       .build();
        mDelegate.addAccount(holder);
        Assert.assertFalse(((AccountManagerFacadeImpl) mFacade).isUpdatePending().get());
        return holder.getAccount();
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccount(AccountHolder.builder(account).build());
    }

    private void assertChildAccountStatus(Account account, @ChildAccountStatus.Status int status) {
        ChildAccountStatusListener listenerMock = mock(ChildAccountStatusListener.class);
        mFacade.checkChildAccountStatus(account, listenerMock);
        verify(listenerMock).onStatusReady(status);
    }
}
