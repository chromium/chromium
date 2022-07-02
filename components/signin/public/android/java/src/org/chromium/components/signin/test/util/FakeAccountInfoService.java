// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * This class is an {@link AccountInfoService} stub intended for testing.
 */
public class FakeAccountInfoService implements IdentityManager.Observer, AccountInfoService {
    private final Map<String, AccountInfo> mAccountInfos =
            Collections.synchronizedMap(new HashMap<>());
    protected final ObserverList<Observer> mObservers;

    public FakeAccountInfoService() {
        mObservers = TestThreadUtils.runOnUiThreadBlockingNoException(ObserverList::new);
    }

    @Override
    public Promise<AccountInfo> getAccountInfoByEmail(String email) {
        return Promise.fulfilled(mAccountInfos.get(email));
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
    public void destroy() {
        mAccountInfos.clear();
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

    /**
     * Adds {@link AccountInfo} with the given information to the fake service.
     */
    public void addAccountInfo(
            String email, String fullName, String givenName, @Nullable Bitmap avatar) {
        addAccountInfo(
                email, fullName, givenName, avatar, new AccountCapabilities(new HashMap<>()));
    }

    /**
     * Adds {@link AccountInfo} with the given information to the fake service.
     */
    public AccountInfo addAccountInfo(String email, String fullName, String givenName,
            @Nullable Bitmap avatar, @NonNull AccountCapabilities capabilities) {
        final CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(
                email, FakeAccountManagerFacade.toGaiaId(email));
        final AccountInfo accountInfo =
                new AccountInfo(coreAccountInfo.getId(), coreAccountInfo.getEmail(),
                        coreAccountInfo.getGaiaId(), fullName, givenName, avatar, capabilities);
        mAccountInfos.put(email, accountInfo);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            for (Observer observer : mObservers) {
                observer.onAccountInfoUpdated(accountInfo);
            }
        });
        return accountInfo;
    }
}
