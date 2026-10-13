// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** IdentityManager provides access to native IdentityManager's public API to java components. */
@NullMarked
public interface IdentityManager {

    /**
     * Represents the state of the refresh tokens loading.
     *
     * <p>TODO(crbug.com/572579970): Move refresh tokens loading state to native IdentityManager. *
     */
    @IntDef({
        RefreshTokensLoadingState.IN_PROGRESS,
        RefreshTokensLoadingState.SUCCEEDED,
        RefreshTokensLoadingState.FAILED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface RefreshTokensLoadingState {
        int IN_PROGRESS = 0;
        int SUCCEEDED = 1;
        int FAILED = 2;
    }

    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    interface Observer {

        /**
         * Called when loading of refresh tokens is finished.
         *
         * @param state The state of refresh token loading. This state is terminal and will never
         *     change back to {@link RefreshTokensLoadingState#IN_PROGRESS}.
         */
        default void onRefreshTokensLoaded(@RefreshTokensLoadingState int state) {}

        /** Called when the given account is updated. */
        default void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {}

        /** Called when the given account is removed. */
        default void onRefreshTokenRemovedForAccount(CoreAccountId accountId) {}

        /**
         * Called for all types of changes to the primary account such as - primary account
         * set/cleared or sync consent granted/revoked in C++.
         *
         * @param eventDetails Details about the primary account change event.
         */
        default void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {}

        /**
         * Called when the Gaia cookie has been deleted explicitly by a user action, e.g. from the
         * settings.
         */
        default void onAccountsCookieDeletedByUserAction() {}

        /** Called after an account is updated. */
        default void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {}
    }

    /** Registers a IdentityManager.Observer */
    void addObserver(Observer observer);

    /** Unregisters a IdentityManager.Observer */
    void removeObserver(Observer observer);

    /** Returns whether the user's primary account is available. */
    boolean hasPrimaryAccount();

    /**
     * Provides access to the core information of the user's primary account. Returns non-null if
     * the primary account was set, null otherwise.
     */
    @Nullable AccountInfo getPrimaryAccountInfo();

    /**
     * Looks up and returns information for account with given |accountId|. If the account cannot be
     * found, return a null value.
     */
    @Nullable AccountInfo findExtendedAccountInfoByAccountId(CoreAccountId accountId);

    /**
     * Looks up and returns information for account with given |email|. If the account cannot be
     * found, return a null value.
     */
    @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email);

    /**
     * Returns an array of all accounts with refresh tokens.
     *
     * <p>Note: Returns an empty array if refresh tokens are not yet fully loaded. Use {@link
     * #getRefreshTokensLoadingState()} to verify the loading state before calling this method.
     */
    List<AccountInfo> getExtendedAccountInfoForAccountsWithRefreshToken();

    /** Returns the state of the refresh tokens loading. */
    @RefreshTokensLoadingState
    int getRefreshTokensLoadingState();

    /** Refreshes extended {@link AccountInfo} with image for all accounts with a refresh token. */
    // TODO(crbug.com/365057341): This doesn't need to be exposed in Java. Move this logic to
    // native.
    void refreshAccountInfoIfStale();

    /** Returns true if the primary account can be cleared/removed from the browser. */
    boolean isClearPrimaryAccountAllowed();

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    void invalidateAccessToken(String accessToken);
}
