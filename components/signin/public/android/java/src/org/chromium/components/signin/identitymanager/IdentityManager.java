// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.List;

/** IdentityManager provides access to native IdentityManager's public API to java components. */
@NullMarked
public interface IdentityManager {
    /**
     * IdentityManager.Observer is notified when the available account information are updated. This
     * is a subset of native's IdentityManager::Observer.
     */
    interface Observer {
        /**
         * Called for all types of changes to the primary account such as - primary account
         * set/cleared or sync consent granted/revoked in C++.
         *
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

    /** Registers a IdentityManager.Observer */
    void addObserver(Observer observer);

    /** Unregisters a IdentityManager.Observer */
    void removeObserver(Observer observer);

    /**
     * Returns whether the user's primary account is available.
     *
     * @param consentLevel {@link ConsentLevel} necessary for the caller.
     */
    boolean hasPrimaryAccount(@ConsentLevel int consentLevel);

    /**
     * Provides access to the core information of the user's primary account. Returns non-null if
     * the primary account was set AND the required consent level was granted, null otherwise.
     *
     * @param consentLevel {@link ConsentLevel} necessary for the caller.
     */
    @Nullable CoreAccountInfo getPrimaryAccountInfo(@ConsentLevel int consentLevel);

    /**
     * Looks up and returns information for account with given |email|. If the account cannot be
     * found, return a null value.
     */
    @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email);

    /**
     * Refreshes extended {@link AccountInfo} with image for all accounts with a refresh token or
     * the given list of {@link AccountInfo} if the existing ones are stale.
     */
    void refreshAccountInfoIfStale(List<AccountInfo> accountInfos);

    /** Returns true if the primary account can be cleared/removed from the browser. */
    boolean isClearPrimaryAccountAllowed();

    /**
     * Called by native to invalidate an OAuth2 token. Please note that the token is invalidated
     * asynchronously.
     */
    @MainThread
    void invalidateAccessToken(String accessToken);
}
