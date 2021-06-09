// Copyright 2021 The Chromium Authors. All rights reserved.
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
public final class AccountInfoServiceImpl implements IdentityManager.Observer, AccountInfoService {
    private static final Object LOCK = new Object();

    @GuardedBy("LOCK")
    private static AccountInfoServiceImpl sInstance;

    private final IdentityManager mIdentityManager;
    private final AccountTrackerService mAccountTrackerService;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private AccountInfoServiceImpl(
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
            sInstance = new AccountInfoServiceImpl(identityManager, accountTrackerService);
            identityManager.addObserver(sInstance);
        }
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
     * Gets the {@link AccountInfo} of the given account email.
     */
    @Override
    public Promise<AccountInfo> getAccountInfoByEmail(String email) {
        final Promise<AccountInfo> accountInfoPromise = new Promise<>();
        mAccountTrackerService.seedAccountsIfNeeded(() -> {
            accountInfoPromise.fulfill(
                    mIdentityManager.findExtendedAccountInfoByEmailAddress(email));
        });
        return accountInfoPromise;
    }

    /**
     * Adds an observer which will be invoked when an {@link AccountInfo} is updated.
     */
    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer which is invoked when an {@link AccountInfo} is updated.
     */
    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Releases the resources used by {@link AccountInfoService}.
     */
    @Override
    public void destroy() {
        mIdentityManager.removeObserver(this);
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
