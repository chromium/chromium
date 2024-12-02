// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.PatternMatcher;
import android.os.Process;
import android.os.SystemClock;

import androidx.annotation.Nullable;

import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.metrics.FetchAccountCapabilitiesFromSystemLibraryResult;

import java.io.IOException;

/**
 * Default implementation of {@link AccountManagerDelegate} which delegates all calls to the
 * Android account manager.
 */
public class SystemAccountManagerDelegate implements AccountManagerDelegate {
    private final AccountManager mAccountManager;
    private AccountsChangeObserver mObserver;

    private static final String TAG = "Auth";

    public SystemAccountManagerDelegate() {
        this(AccountManager.get(ContextUtils.getApplicationContext()));
    }

    SystemAccountManagerDelegate(AccountManager accountManager) {
        mAccountManager = accountManager;
        mObserver = null;
    }

    @Override
    public void attachAccountsChangeObserver(AccountsChangeObserver observer) {
        assert mObserver == null : "Another AccountsChangeObserver is already attached!";

        mObserver = observer;
        Context context = ContextUtils.getApplicationContext();
        BroadcastReceiver receiver =
                new BroadcastReceiver() {
                    @Override
                    public void onReceive(final Context context, final Intent intent) {
                        mObserver.onCoreAccountInfosChanged();
                    }
                };
        IntentFilter accountsChangedIntentFilter = new IntentFilter();
        accountsChangedIntentFilter.addAction(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION);
        ContextUtils.registerProtectedBroadcastReceiver(
                context, receiver, accountsChangedIntentFilter);

        IntentFilter gmsPackageReplacedFilter = new IntentFilter();
        gmsPackageReplacedFilter.addAction(Intent.ACTION_PACKAGE_REPLACED);
        gmsPackageReplacedFilter.addDataScheme("package");
        gmsPackageReplacedFilter.addDataPath(
                "com.google.android.gms", PatternMatcher.PATTERN_PREFIX);

        ContextUtils.registerProtectedBroadcastReceiver(
                context, receiver, gmsPackageReplacedFilter);
    }

    @Override
    public Account[] getAccountsSynchronous() throws AccountManagerDelegateException {
        if (!isGooglePlayServicesAvailable()) {
            throw new AccountManagerDelegateException("Can't use Google Play Services");
        }
        if (hasGetAccountsPermission()) {
            long startTime = SystemClock.elapsedRealtime();
            Account[] accounts =
                    mAccountManager.getAccountsByType(GoogleAuthUtil.GOOGLE_ACCOUNT_TYPE);
            RecordHistogram.recordTimesHistogram(
                    "Signin.AndroidGetAccountsTime_AccountManager",
                    SystemClock.elapsedRealtime() - startTime);
            return accounts;
        }
        // Don't report any accounts if we don't have permission.
        // TODO(crbug.com/40942462): Throw an exception if permission was denied.
        return new Account[] {};
    }

    @Override
    public AccessTokenData getAuthToken(Account account, String authTokenScope)
            throws AuthException {
        ThreadUtils.assertOnBackgroundThread();
        assert AccountUtils.GOOGLE_ACCOUNT_TYPE.equals(account.type);
        try {
            return new AccessTokenData(
                    GoogleAuthUtil.getTokenWithNotification(
                            ContextUtils.getApplicationContext(), account, authTokenScope, null));
        } catch (GoogleAuthException ex) {
            // This case includes a UserRecoverableNotifiedException, but most clients will have
            // their own retry mechanism anyway.
            throw new AuthException(
                    AuthException.NONTRANSIENT,
                    "Error while getting token for scope '" + authTokenScope + "'",
                    ex);
        } catch (IOException ex) {
            throw new AuthException(AuthException.TRANSIENT, ex);
        }
    }

    @Override
    public void invalidateAuthToken(String authToken) throws AuthException {
        try {
            GoogleAuthUtil.clearToken(ContextUtils.getApplicationContext(), authToken);
        } catch (GoogleAuthException ex) {
            throw new AuthException(AuthException.NONTRANSIENT, ex);
        } catch (IOException ex) {
            throw new AuthException(AuthException.TRANSIENT, ex);
        }
    }

    protected boolean hasFeatures(Account account, String[] features) {
        if (hasGetAccountsPermission()) {
            try {
                return mAccountManager.hasFeatures(account, features, null, null).getResult();
            } catch (AuthenticatorException | IOException | OperationCanceledException e) {
                Log.e(TAG, "Error while checking features: ", e);
            }
        }
        return false;
    }

    @Override
    public boolean hasFeature(Account account, String feature) {
        return hasFeatures(account, new String[] {feature});
    }

    @Override
    public @CapabilityResponse int hasCapability(Account account, String capability) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.AccountCapabilities.GetFromSystemLibraryResult",
                FetchAccountCapabilitiesFromSystemLibraryResult.API_NOT_AVAILABLE,
                FetchAccountCapabilitiesFromSystemLibraryResult.MAX_VALUE + 1);
        return CapabilityResponse.EXCEPTION;
    }

    // No permission is needed on 23+ and Chrome always has MANAGE_ACCOUNTS permission on lower APIs
    @SuppressLint("MissingPermission")
    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
        AccountManagerCallback<Bundle> accountManagerCallback =
                accountManagerFuture -> {
                    try {
                        Bundle bundle = accountManagerFuture.getResult();
                        callback.onResult(bundle.getParcelable(AccountManager.KEY_INTENT));
                    } catch (OperationCanceledException | IOException | AuthenticatorException e) {
                        Log.e(TAG, "Error while creating an intent to add an account: ", e);
                        callback.onResult(null);
                    }
                };
        mAccountManager.addAccount(
                GoogleAuthUtil.GOOGLE_ACCOUNT_TYPE,
                null,
                null,
                null,
                null,
                accountManagerCallback,
                null);
    }

    // No permission is needed on 23+ and Chrome always has MANAGE_ACCOUNTS permission on lower APIs
    @SuppressLint("MissingPermission")
    @Override
    public void updateCredentials(
            Account account, Activity activity, final Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        AccountManagerCallback<Bundle> realCallback =
                future -> {
                    Bundle bundle = null;
                    try {
                        bundle = future.getResult();
                    } catch (AuthenticatorException | IOException e) {
                        Log.e(TAG, "Error while update credentials: ", e);
                    } catch (OperationCanceledException e) {
                        Log.w(TAG, "Updating credentials was cancelled.");
                    }
                    boolean success =
                            bundle != null
                                    && bundle.getString(AccountManager.KEY_ACCOUNT_TYPE) != null;
                    if (callback != null) {
                        callback.onResult(success);
                    }
                };
        // Android 4.4 throws NullPointerException if null is passed
        Bundle emptyOptions = new Bundle();
        mAccountManager.updateCredentials(
                account, "android", emptyOptions, activity, realCallback, null);
    }

    @Nullable
    @Override
    public String getAccountGaiaId(String accountEmail) {
        try {
            return GoogleAuthUtil.getAccountId(ContextUtils.getApplicationContext(), accountEmail);
        } catch (IOException | GoogleAuthException ex) {
            Log.e(TAG, "SystemAccountManagerDelegate.getAccountGaiaId", ex);
            return null;
        }
    }

    @Override
    public void confirmCredentials(Account account, Activity activity, Callback<Bundle> callback) {
        AccountManagerCallback<Bundle> accountManagerCallback =
                (accountManagerFuture) -> {
                    @Nullable Bundle result = null;
                    try {
                        result = accountManagerFuture.getResult();
                    } catch (Exception e) {
                        Log.e(TAG, "Error while confirming credentials: ", e);
                    }
                    callback.onResult(result);
                };
        mAccountManager.confirmCredentials(
                account, new Bundle(), activity, accountManagerCallback, null);
    }

    protected boolean isGooglePlayServicesAvailable() {
        return ExternalAuthUtils.getInstance().canUseGooglePlayServices();
    }

    protected boolean hasGetAccountsPermission() {
        return ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(),
                        Manifest.permission.GET_ACCOUNTS,
                        Process.myPid(),
                        Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }
}
