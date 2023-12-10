// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import com.google.android.gms.auth.GoogleAuthUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** Robolectric tests for {@link SystemAccountManagerDelegateTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SystemAccountManagerDelegateTest {
    private static final Account[] GOOGLE_ACCOUNTS = {
        AccountUtils.createAccountFromName("account-name"),
        AccountUtils.createAccountFromName("other-account-name")
    };
    @Mock private AccountManager mAccountManager;
    @Mock private AccountManagerFuture<Bundle> mAccountManagerFuture;
    @Mock private Account mAccount;
    @Mock private Activity mActivity;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private Context mContext;

    private final AtomicReference<Bundle> mConfirmCredentialsResponse = new AtomicReference<>();
    private SystemAccountManagerDelegate mDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mAccountManager = Mockito.mock(AccountManager.class);
        doReturn(GOOGLE_ACCOUNTS)
                .when(mAccountManager)
                .getAccountsByType(eq(GoogleAuthUtil.GOOGLE_ACCOUNT_TYPE));

        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        when(mContext.checkPermission(eq(Manifest.permission.GET_ACCOUNTS), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        ContextUtils.initApplicationContextForTests(mContext);
        mDelegate = new SystemAccountManagerDelegate(mAccountManager);
    }

    @Test
    public void testConfirmCredentials_successfulResult_triggersCallback() throws Exception {
        Bundle bundle = new Bundle();
        doReturn(bundle).when(mAccountManagerFuture).getResult();

        doAnswer(
                        (invocation) -> {
                            AccountManagerCallback<Bundle> callback = invocation.getArgument(3);
                            callback.run(mAccountManagerFuture);
                            return null;
                        })
                .when(mAccountManager)
                .confirmCredentials(any(), any(), any(), any(), any());

        mDelegate.confirmCredentials(mAccount, mActivity, mConfirmCredentialsResponse::set);
        assertEquals(bundle, mConfirmCredentialsResponse.get());
        verify(mAccountManager, times(1))
                .confirmCredentials(
                        eq(mAccount), any(Bundle.class), eq(mActivity), any(), eq(null));
    }

    @Test
    public void testConfirmCredentials_exceptionOnResult_triggersCallback() throws Exception {
        doThrow(IOException.class).when(mAccountManagerFuture).getResult();

        doAnswer(
                        (invocation) -> {
                            AccountManagerCallback<Bundle> callback = invocation.getArgument(3);
                            callback.run(mAccountManagerFuture);
                            return null;
                        })
                .when(mAccountManager)
                .confirmCredentials(any(), any(), any(), any(), any());

        mDelegate.confirmCredentials(mAccount, mActivity, mConfirmCredentialsResponse::set);
        assertNull(mConfirmCredentialsResponse.get());
        verify(mAccountManager, times(1))
                .confirmCredentials(
                        eq(mAccount), any(Bundle.class), eq(mActivity), any(), eq(null));
    }

    @Test
    public void testGetAccountsSynchronous_returnsAccounts() throws Exception {
        Account[] accounts = mDelegate.getAccountsSynchronous();

        assertSame(GOOGLE_ACCOUNTS, accounts);
    }

    @Test
    public void testGetAccountsSynchronous_doesNotHavePermission_returnsEmptyList()
            throws Exception {
        when(mContext.checkPermission(eq(Manifest.permission.GET_ACCOUNTS), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_DENIED);

        Account[] accounts = mDelegate.getAccountsSynchronous();

        assertEquals(0, accounts.length);
    }

    @Test
    public void testGetAccountsSynchronous_throwsException() {
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(false);

        assertThrows(
                AccountManagerDelegateException.class, () -> mDelegate.getAccountsSynchronous());
    }
}
