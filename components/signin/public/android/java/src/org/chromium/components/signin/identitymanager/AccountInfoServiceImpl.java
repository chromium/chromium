// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.components.signin.base.AccountInfo;

/** This class handles the {@link AccountInfo} fetch on Java side. */
final class AccountInfoServiceImpl implements IdentityManager.Observer, AccountInfoService {
    private final IdentityManager mIdentityManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    AccountInfoServiceImpl(IdentityManager identityManager) {
        mIdentityManager = identityManager;
        identityManager.addObserver(this);
    }

    /** Gets the {@link AccountInfo} of the given account email. */
    @Override
    public Promise<AccountInfo> getAccountInfoByEmail(String email) {
        final Promise<AccountInfo> accountInfoPromise = new Promise<>();
        accountInfoPromise.fulfill(mIdentityManager.findExtendedAccountInfoByEmailAddress(email));
        return accountInfoPromise;
    }

    /** Adds an observer which will be invoked when an {@link AccountInfo} is updated. */
    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer which is invoked when an {@link AccountInfo} is updated. */
    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Releases the resources used by {@link AccountInfoService}. */
    @Override
    public void destroy() {
        mIdentityManager.removeObserver(this);
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        for (Observer observer : mObservers) {
            observer.onAccountInfoUpdated(accountInfo);
        }
    }
}
