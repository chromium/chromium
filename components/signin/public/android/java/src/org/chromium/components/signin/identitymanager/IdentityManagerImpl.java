// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;

import java.util.Arrays;
import java.util.List;

/** IdentityManager provides access to native IdentityManager's public API to java components. */
@NullMarked
@JNINamespace("signin")
public class IdentityManagerImpl implements IdentityManager {
    private long mNativeIdentityManager;
    private final ProfileOAuth2TokenServiceDelegate mProfileOAuth2TokenServiceDelegate;
    // TODO(crbug.com/572579970): Move refresh tokens loading state to native IdentityManager.
    private @RefreshTokensLoadingState int mRefreshTokensLoadingState =
            RefreshTokensLoadingState.IN_PROGRESS;

    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /** Called by native to create an instance of IdentityManager. */
    @CalledByNative
    @VisibleForTesting
    public static IdentityManagerImpl create(
            long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate,
            IdentityMutator identityMutator) {
        return new IdentityManagerImpl(
                nativeIdentityManager, profileOAuth2TokenServiceDelegate, identityMutator);
    }

    private IdentityManagerImpl(
            long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate,
            IdentityMutator identityMutator) {
        assert nativeIdentityManager != 0;
        mNativeIdentityManager = nativeIdentityManager;
        mProfileOAuth2TokenServiceDelegate = profileOAuth2TokenServiceDelegate;
        identityMutator.setIdentityManager(this);
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
    public boolean hasPrimaryAccount() {
        return getPrimaryAccountInfo() != null;
    }

    @Override
    public @Nullable AccountInfo getPrimaryAccountInfo() {
        return IdentityManagerImplJni.get().getPrimaryAccountInfo(mNativeIdentityManager);
    }

    @Override
    public @Nullable AccountInfo findExtendedAccountInfoByAccountId(CoreAccountId accountId) {
        return IdentityManagerImplJni.get()
                .findExtendedAccountInfoByAccountId(mNativeIdentityManager, accountId);
    }

    @Override
    public @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email) {
        return IdentityManagerImplJni.get()
                .findExtendedAccountInfoByEmailAddress(mNativeIdentityManager, email);
    }

    @Override
    public void refreshAccountInfoIfStale() {
        IdentityManagerImplJni.get().refreshAccountInfoIfStale(mNativeIdentityManager);
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

    /** Provides the information of all accounts that have refresh tokens. */
    @Override
    public List<AccountInfo> getExtendedAccountInfoForAccountsWithRefreshToken() {
        final var accounts =
                IdentityManagerImplJni.get()
                        .getExtendedAccountInfoForAccountsWithRefreshToken(mNativeIdentityManager);
        assert accounts != null;
        return Arrays.asList(accounts);
    }

    /**
     * Returns the state of the refresh tokens loading.
     *
     * <p>TODO(crbug.com/572579970): Move refresh tokens loading state to native IdentityManager.
     */
    @Override
    public @RefreshTokensLoadingState int getRefreshTokensLoadingState() {
        ThreadUtils.assertOnUiThread();
        if (IdentityManagerImplJni.get().areRefreshTokensLoaded(mNativeIdentityManager)) {
            mRefreshTokensLoadingState = RefreshTokensLoadingState.SUCCEEDED;
        }
        return mRefreshTokensLoadingState;
    }

    /**
     * Sets the refresh tokens loading state to failed and notifies observers. This method must only
     * be called when the refresh tokens loading state is IN_PROGRESS.
     *
     * <p>TODO(crbug.com/572579970): Move refresh tokens loading state to native IdentityManager.
     */
    void setRefreshTokensLoadingFailed() {
        ThreadUtils.assertOnUiThread();
        assert SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
        if (getRefreshTokensLoadingState() != RefreshTokensLoadingState.IN_PROGRESS) {
            throw new IllegalStateException(
                    "Setting refresh tokens loading state to FAILED is only allowed when the"
                            + " current state is IN_PROGRESS, while the current state is "
                            + mRefreshTokensLoadingState);
        }
        mRefreshTokensLoadingState = RefreshTokensLoadingState.FAILED;
        for (Observer observer : mObservers) {
            observer.onRefreshTokensLoaded(mRefreshTokensLoadingState);
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

    /** Called when all refresh tokens are loaded. */
    @CalledByNative
    private void onRefreshTokensLoaded() {
        mRefreshTokensLoadingState = RefreshTokensLoadingState.SUCCEEDED;
        for (Observer observer : mObservers) {
            observer.onRefreshTokensLoaded(mRefreshTokensLoadingState);
        }
    }

    /** Called when the refresh token of the give account gets updated. */
    @CalledByNative
    private void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {
        for (Observer observer : mObservers) {
            observer.onRefreshTokenUpdatedForAccount(coreAccountInfo);
        }
    }

    @CalledByNative
    private void onRefreshTokenRemovedForAccount(CoreAccountId accountId) {
        for (Observer observer : mObservers) {
            observer.onRefreshTokenRemovedForAccount(accountId);
        }
    }

    @CalledByNative
    private void onAccountsCookieDeletedByUserAction() {
        for (Observer observer : mObservers) {
            observer.onAccountsCookieDeletedByUserAction();
        }
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

        @Nullable AccountInfo getPrimaryAccountInfo(long nativeIdentityManager);

        @Nullable AccountInfo findExtendedAccountInfoByAccountId(
                long nativeIdentityManager, CoreAccountId accountId);

        @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(
                long nativeIdentityManager, String email);

        @JniType("std::vector<AccountInfo>")
        AccountInfo[] getExtendedAccountInfoForAccountsWithRefreshToken(long nativeIdentityManager);

        void refreshAccountInfoIfStale(long nativeIdentityManager);

        boolean isClearPrimaryAccountAllowed(long nativeIdentityManager);

        boolean areRefreshTokensLoaded(long nativeIdentityManager);
    }
}
