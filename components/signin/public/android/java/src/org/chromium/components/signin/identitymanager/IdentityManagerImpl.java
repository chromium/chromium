// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;

import java.util.List;

/** IdentityManager provides access to native IdentityManager's public API to java components. */
@NullMarked
public class IdentityManagerImpl implements IdentityManager {
    private long mNativeIdentityManager;
    private final ProfileOAuth2TokenServiceDelegate mProfileOAuth2TokenServiceDelegate;

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private @Nullable Callback<CoreAccountInfo> mRefreshTokenUpdateObserver;

    /** Called by native to create an instance of IdentityManager. */
    @CalledByNative
    @VisibleForTesting
    public static IdentityManagerImpl create(
            long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate) {
        return new IdentityManagerImpl(nativeIdentityManager, profileOAuth2TokenServiceDelegate);
    }

    private IdentityManagerImpl(
            long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate) {
        assert nativeIdentityManager != 0;
        mNativeIdentityManager = nativeIdentityManager;
        mProfileOAuth2TokenServiceDelegate = profileOAuth2TokenServiceDelegate;
    }

    /** Called by native upon KeyedService's shutdown */
    @CalledByNative
    private void destroy() {
        mNativeIdentityManager = 0;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean hasPrimaryAccount(@ConsentLevel int consentLevel) {
        return getPrimaryAccountInfo(consentLevel) != null;
    }

    @Override
    public @Nullable CoreAccountInfo getPrimaryAccountInfo(@ConsentLevel int consentLevel) {
        return IdentityManagerImplJni.get()
                .getPrimaryAccountInfo(mNativeIdentityManager, consentLevel);
    }

    @Override
    public @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email) {
        return IdentityManagerImplJni.get()
                .findExtendedAccountInfoByEmailAddress(mNativeIdentityManager, email);
    }

    @Override
    public void refreshAccountInfoIfStale(List<AccountInfo> accountInfos) {
        for (AccountInfo accountInfo : accountInfos) {
            IdentityManagerImplJni.get()
                    .refreshAccountInfoIfStale(mNativeIdentityManager, accountInfo.getId());
        }
    }

    @Override
    public boolean isClearPrimaryAccountAllowed() {
        return IdentityManagerImplJni.get().isClearPrimaryAccountAllowed(mNativeIdentityManager);
    }

    @Override
    @MainThread
    public void invalidateAccessToken(String accessToken) {
        assert mProfileOAuth2TokenServiceDelegate != null;

        // TODO(crbug.com/40615112) The following should call a JNI method instead.
        mProfileOAuth2TokenServiceDelegate.invalidateAccessToken(accessToken);
    }

    /** Called after an account is updated. */
    @CalledByNative
    @VisibleForTesting
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onExtendedAccountInfoUpdated(accountInfo);
        }
    }

    /**
     * Called for all types of changes to the primary account such as - primary account set/cleared
     * or sync consent granted/revoked in C++.
     */
    @CalledByNative
    private void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountChanged(eventDetails);
        }
    }

    /** Called when the refresh token of the give account gets updated. */
    @CalledByNative
    private void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {
        if (mRefreshTokenUpdateObserver != null) {
            mRefreshTokenUpdateObserver.onResult(coreAccountInfo);
        }
    }

    @CalledByNative
    private void onAccountsCookieDeletedByUserAction() {
        for (Observer observer : mObservers) {
            observer.onAccountsCookieDeletedByUserAction();
        }
    }

    /** Provides the information of all accounts that have refresh tokens. */
    @VisibleForTesting
    public CoreAccountInfo[] getAccountsWithRefreshTokens() {
        return IdentityManagerImplJni.get().getAccountsWithRefreshTokens(mNativeIdentityManager);
    }

    public void setRefreshTokenUpdateObserverForTests(Callback<CoreAccountInfo> callback) {
        mRefreshTokenUpdateObserver = callback;
    }

    /** Can be called by native code to convert from Java to the corresponding C++ object. */
    @CalledByNative
    private long getNativePointer() {
        return mNativeIdentityManager;
    }

    @MainThread
    public void updateAuthErrorForTesting(
            CoreAccountId accountId, GoogleServiceAuthError authError) {
        assert mProfileOAuth2TokenServiceDelegate != null;

        mProfileOAuth2TokenServiceDelegate.updateAuthErrorForTesting(accountId, authError);
    }

    @NativeMethods
    public interface Natives {

        @Nullable CoreAccountInfo getPrimaryAccountInfo(
                long nativeIdentityManager, int consentLevel);

        @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(
                long nativeIdentityManager, String email);

        CoreAccountInfo[] getAccountsWithRefreshTokens(long nativeIdentityManager);

        // TODO(crbug.com/40284908): Remove the accountId parameter.
        void refreshAccountInfoIfStale(
                long nativeIdentityManager, @JniType("CoreAccountId") CoreAccountId accountId);

        boolean isClearPrimaryAccountAllowed(long nativeIdentityManager);
    }
}
