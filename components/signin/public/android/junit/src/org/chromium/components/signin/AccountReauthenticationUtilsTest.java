// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for the {@link AccountReauthenticationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccountReauthenticationUtilsTest {
    private static final long MOCK_RECENT_TIME_WINDOW_MILLIS = 10 * 60 * 1000; // 10 minutes

    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private AccountManagerFacade mAccountManagerFacade;
    @Mock private Account mAccount;

    private final @AccountReauthenticationUtils.RecentAuthenticationResult AtomicReference<Integer>
            mRecentAuthenticationResult = new AtomicReference<>();
    private final @AccountReauthenticationUtils.ConfirmationResult AtomicReference<Integer>
            mRecentConfirmationResult = new AtomicReference<>();

    private final Answer<Object> mRecentAuthenticationAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                Bundle response = new Bundle();
                response.putLong(AccountManager.KEY_LAST_AUTHENTICATED_TIME, getRecentTimestamp());
                callback.onResult(response);
                return null;
            };

    private final Answer<Object> mOldAuthenticationAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                Bundle response = new Bundle();
                response.putLong(AccountManager.KEY_LAST_AUTHENTICATED_TIME, getOldTimestamp());
                callback.onResult(response);
                return null;
            };

    private final Answer<Object> mConfirmationSuccessAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                Bundle response = new Bundle();
                response.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
                callback.onResult(response);
                return null;
            };

    private final Answer<Object> mConfirmationRejectedAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                Bundle response = new Bundle();
                response.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, false);
                callback.onResult(response);
                return null;
            };

    private final Answer<Object> mEmptyBundleAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                callback.onResult(new Bundle());
                return null;
            };

    private final Answer<Object> mErrorAnswer =
            (invocation) -> {
                Callback<Bundle> callback = invocation.getArgument(2);
                callback.onResult(null);
                return null;
            };

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testConfirmRecentAuthentication_recentAuthentication_triggersCallback() {
        doAnswer(mRecentAuthenticationAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());

        new AccountReauthenticationUtils()
                .confirmRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        mRecentAuthenticationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer)
                        AccountReauthenticationUtils.RecentAuthenticationResult
                                .HAS_RECENT_AUTHENTICATION,
                mRecentAuthenticationResult.get());
    }

    @Test
    public void testConfirmRecentAuthentication_oldAuthentication_triggersCallback() {
        doAnswer(mOldAuthenticationAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());

        new AccountReauthenticationUtils()
                .confirmRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        mRecentAuthenticationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer)
                        AccountReauthenticationUtils.RecentAuthenticationResult
                                .NO_RECENT_AUTHENTICATION,
                mRecentAuthenticationResult.get());
    }

    @Test
    public void testConfirmRecentAuthentication_noPreviousAuthentication_triggersCallback() {
        doAnswer(mEmptyBundleAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());

        new AccountReauthenticationUtils()
                .confirmRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        mRecentAuthenticationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer)
                        AccountReauthenticationUtils.RecentAuthenticationResult
                                .NO_RECENT_AUTHENTICATION,
                mRecentAuthenticationResult.get());
    }

    @Test
    public void testConfirmRecentAuthentication_nullResponse_triggersCallback() {
        doAnswer(mErrorAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());

        new AccountReauthenticationUtils()
                .confirmRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        mRecentAuthenticationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer)
                        AccountReauthenticationUtils.RecentAuthenticationResult
                                .RECENT_AUTHENTICATION_ERROR,
                mRecentAuthenticationResult.get());
    }

    @Test
    public void
            testConfirmCredentialsOrRecentAuthentication_confirmationSuccess_triggersCallback() {
        doAnswer(mEmptyBundleAnswer)
                .doAnswer(mConfirmationSuccessAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());
        HistogramWatcher accountReauthenticationHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AccountReauthenticationUtils.ACCOUNT_REAUTHENTICATION_HISTOGRAM,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.STARTED,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.SUCCESS)
                        .build();

        new AccountReauthenticationUtils()
                .confirmCredentialsOrRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        null,
                        mRecentConfirmationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer) AccountReauthenticationUtils.ConfirmationResult.SUCCESS,
                mRecentConfirmationResult.get());
        accountReauthenticationHistogram.assertExpected();
    }

    @Test
    public void
            testConfirmCredentialsOrRecentAuthentication_recentAuthentication_triggersCallback() {
        doAnswer(mRecentAuthenticationAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());
        HistogramWatcher accountReauthenticationHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AccountReauthenticationUtils.ACCOUNT_REAUTHENTICATION_HISTOGRAM,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.STARTED,
                                AccountReauthenticationUtils.AccountReauthenticationEvent
                                        .SUCCESS_RECENT_AUTHENTICATION)
                        .build();

        new AccountReauthenticationUtils()
                .confirmCredentialsOrRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        null,
                        mRecentConfirmationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer) AccountReauthenticationUtils.ConfirmationResult.SUCCESS,
                mRecentConfirmationResult.get());
        accountReauthenticationHistogram.assertExpected();
    }

    @Test
    public void
            testConfirmCredentialsOrRecentAuthentication_confirmationRejected_triggersCallback() {
        doAnswer(mEmptyBundleAnswer)
                .doAnswer(mConfirmationRejectedAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());
        HistogramWatcher accountReauthenticationHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AccountReauthenticationUtils.ACCOUNT_REAUTHENTICATION_HISTOGRAM,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.STARTED,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.REJECTED)
                        .build();

        new AccountReauthenticationUtils()
                .confirmCredentialsOrRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        null,
                        mRecentConfirmationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer) AccountReauthenticationUtils.ConfirmationResult.REJECTED,
                mRecentConfirmationResult.get());
        accountReauthenticationHistogram.assertExpected();
    }

    @Test
    public void testConfirmCredentialsOrRecentAuthentication_confirmationError_triggersCallback() {
        doAnswer(mEmptyBundleAnswer)
                .doAnswer(mErrorAnswer)
                .when(mAccountManagerFacade)
                .confirmCredentials(any(Account.class), any(), any());
        HistogramWatcher accountReauthenticationHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                AccountReauthenticationUtils.ACCOUNT_REAUTHENTICATION_HISTOGRAM,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.STARTED,
                                AccountReauthenticationUtils.AccountReauthenticationEvent.ERROR)
                        .build();

        new AccountReauthenticationUtils()
                .confirmCredentialsOrRecentAuthentication(
                        mAccountManagerFacade,
                        mAccount,
                        null,
                        mRecentConfirmationResult::set,
                        MOCK_RECENT_TIME_WINDOW_MILLIS);
        assertEquals(
                (Integer) AccountReauthenticationUtils.ConfirmationResult.ERROR,
                mRecentConfirmationResult.get());
        accountReauthenticationHistogram.assertExpected();
    }

    /**
     * Get a time just within the recent time window. Note: that this will always return the same
     * time due to the FakeTimeTestRule.
     */
    private Long getRecentTimestamp() {
        return TimeUtils.currentTimeMillis() - (TimeUnit.MINUTES.toMillis(10) - 1);
    }

    /**
     * Get a time just outside the recent time window. Note: that this will always return the same
     * time due to the FakeTimeTestRule.
     */
    private Long getOldTimestamp() {
        return TimeUtils.currentTimeMillis() - (TimeUnit.MINUTES.toMillis(10) + 1);
    }
}
