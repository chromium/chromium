// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.components.signin.base.AccountInfo;

/**
 * This class handles the {@link AccountInfo} fetch on Java side.
 */
public final class AccountInfoService implements IdentityManager.Observer {
    /**
     * Observes the changes of {@link AccountInfo}.
     */
    public interface Observer {
        /**
         * Notifies when an {@link AccountInfo} is updated.
         */
        void onAccountInfoUpdated(AccountInfo accountInfo);
    }

    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static AccountInfoService sInstance;

    private final IdentityManager mIdentityManager;
    private final AccountTrackerService mAccountTrackerService;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private AccountInfoService(
            IdentityManager identityManager, AccountTrackerService accountTrackerService) {
        mIdentityManager = identityManager;
        mAccountTrackerService = accountTrackerService;
    }

    /**
     * Initializes the singleton object.
     */
    public static void init(
            IdentityManager identityManager, AccountTrackerService accountTrackerService) {
        synchronized (LOCK) {
            sInstance = new AccountInfoService(identityManager, accountTrackerService);
            identityManager.addObserver(sInstance);
        }
    }

    /**
     * Releases the resources used by {@link AccountInfoService}.
     */
    public void destroy() {
        mIdentityManager.removeObserver(this);
    }

    /**
     * Gets the singleton instance.
     */
    public static AccountInfoService get() {
        synchronized (LOCK) {
            if (sInstance == null) {
                throw new RuntimeException("The AccountInfoService is not yet initialized!");
            }
            return sInstance;
        }
    }

    @VisibleForTesting
    public static void resetForTests() {
        synchronized (LOCK) {
            sInstance = null;
        }
    }

    /**
     * Gets the corresponding {@link AccountInfo} of the given account email asynchronously.
     */
    public Promise<AccountInfo> getAccountInfoByEmailAsync(String email) {
        final Promise<AccountInfo> accountInfoPromise = new Promise<>();
        mAccountTrackerService.seedAccountsIfNeeded(() -> {
            accountInfoPromise.fulfill(
                    mIdentityManager
                            .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                                    email));
        });
        return accountInfoPromise;
    }

    /**
     * Adds an observer which will be invoked when an {@link AccountInfo} is updated.
     */
    public void addObserver(Observer onAccountInfoUpdated) {
        mObservers.addObserver(onAccountInfoUpdated);
    }

    /**
     * Removes an observer which is invoked when an {@link AccountInfo} is updated.
     */
    public void removeObserver(Observer onAccountInfoUpdated) {
        mObservers.removeObserver(onAccountInfoUpdated);
    }

    /**
     * Implements {@link IdentityManager.Observer}.
     */
    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onAccountInfoUpdated(accountInfo);
        }
    }
}
