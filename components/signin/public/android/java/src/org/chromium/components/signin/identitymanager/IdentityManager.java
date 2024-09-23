// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.List;

/** IdentityManager provides access to native IdentityManager's public API to java components. */
public class IdentityManager {
    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    public interface Observer {
        /**
         * Called for all types of changes to the primary account such as - primary account
         * set/cleared or sync consent granted/revoked in C++.
         * @param eventDetails Details about the primary account change event.
         */
        default void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {}

        /**
         * Called when the Gaia cookie has been deleted explicitly by a user action, e.g. from
         * the settings.
         */
        default void onAccountsCookieDeletedByUserAction() {}

        /** Called after an account is updated. */
        default void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {}
    }

    private long mNativeIdentityManager;
    private final ProfileOAuth2TokenServiceDelegate mProfileOAuth2TokenServiceDelegate;

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private Callback<CoreAccountInfo> mRefreshTokenUpdateObserver;

    /** Called by native to create an instance of IdentityManager. */
    @CalledByNative
    @VisibleForTesting
    public static IdentityManager create(
            long nativeIdentityManager,
            ProfileOAuth2TokenServiceDelegate profileOAuth2TokenServiceDelegate) {
        return new IdentityManager(nativeIdentityManager, profileOAuth2TokenServiceDelegate);
    }

    private IdentityManager(
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

    /** Registers a IdentityManager.Observer */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Unregisters a IdentityManager.Observer */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Called for all types of changes to the primary account such as - primary account set/cleared
     * or sync consent granted/revoked in C++.
     */
    @CalledByNative
    @VisibleForTesting
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        for (Observer observer : mObservers) {
            observer.onPrimaryAccountChanged(eventDetails);
        }
    }

    @CalledByNative
    @VisibleForTesting
    public void onAccountsCookieDeletedByUserAction() {
        for (Observer observer : mObservers) {
            observer.onAccountsCookieDeletedByUserAction();
        }
    }

    /** Called after an account is updated. */
    @CalledByNative
    @VisibleForTesting
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onExtendedAccountInfoUpdated(accountInfo);
        }
    }

    /** Called when the refresh token of the give account gets updated. */
    @CalledByNative
    private void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {
        if (mRefreshTokenUpdateObserver != null) {
            mRefreshTokenUpdateObserver.onResult(coreAccountInfo);
        }
    }

    /**
     * Returns whether the user's primary account is available.
     * @param consentLevel {@link ConsentLevel} necessary for the caller.
     */
    public boolean hasPrimaryAccount(@ConsentLevel int consentLevel) {
        return getPrimaryAccountInfo(consentLevel) != null;
    }

    /** Provides the information of all accounts that have refresh tokens. */
    @VisibleForTesting
    public CoreAccountInfo[] getAccountsWithRefreshTokens() {
        return IdentityManagerJni.get().getAccountsWithRefreshTokens(mNativeIdentityManager);
    }

    /**
     * Provides access to the core information of the user's primary account.
     * Returns non-null if the primary account was set AND the required consent level was granted,
     * null otherwise.
     *
     * @param consentLevel {@link ConsentLevel} necessary for the caller.
     */
    public @Nullable CoreAccountInfo getPrimaryAccountInfo(@ConsentLevel int consentLevel) {
        return IdentityManagerJni.get().getPrimaryAccountInfo(mNativeIdentityManager, consentLevel);
    }

    /**
     * Looks up and returns information for account with given |email|. If the account
     * cannot be found, return a null value.
     */
    public @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email) {
        return IdentityManagerJni.get()
                .findExtendedAccountInfoByEmailAddress(mNativeIdentityManager, email);
    }

    /**
     * Refreshes extended {@link AccountInfo} with image for all accounts with a refresh token or
     * the given list of {@link CoreAccountInfo} if the existing ones are stale.
     */
    public void refreshAccountInfoIfStale(List<CoreAccountInfo> accountInfos) {
        for (CoreAccountInfo accountInfo : accountInfos) {
            IdentityManagerJni.get()
                    .refreshAccountInfoIfStale(mNativeIdentityManager, accountInfo.getId());
        }
    }

    /** Returns true if the primary account can be cleared/removed from the browser. */
    public boolean isClearPrimaryAccountAllowed() {
        return IdentityManagerJni.get().isClearPrimaryAccountAllowed(mNativeIdentityManager);
    }

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    public void invalidateAccessToken(String accessToken) {
        assert mProfileOAuth2TokenServiceDelegate != null;

        // TODO(crbug.com/40615112) The following should call a JNI method instead.
        mProfileOAuth2TokenServiceDelegate.invalidateAccessToken(accessToken);
    }

    public void setRefreshTokenUpdateObserverForTests(Callback<CoreAccountInfo> callback) {
        mRefreshTokenUpdateObserver = callback;
    }

    @NativeMethods
    public interface Natives {
        @Nullable
        CoreAccountInfo getPrimaryAccountInfo(long nativeIdentityManager, int consentLevel);

        @Nullable
        AccountInfo findExtendedAccountInfoByEmailAddress(long nativeIdentityManager, String email);

        CoreAccountInfo[] getAccountsWithRefreshTokens(long nativeIdentityManager);

        // TODO(crbug.com/40284908): Remove the accountId parameter.
        void refreshAccountInfoIfStale(long nativeIdentityManager, CoreAccountId accountId);

        boolean isClearPrimaryAccountAllowed(long nativeIdentityManager);
    }
}
