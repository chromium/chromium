// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;


import androidx.annotation.MainThread;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Fake implementation of {@link IdentityManager} for testing. */
@NullMarked
public class FakeIdentityManager implements IdentityManager {
    private final List<Observer> mObservers = new ArrayList<>();
    private final Map<CoreAccountId, AccountInfo> mExtendedAccountInfos = new LinkedHashMap<>();
    private @Nullable CoreAccountInfo mPrimaryAccount;
    private boolean mIsOnExtendedAccountInfoUpdatedBlocked;
    private boolean mIsClearPrimaryAccountAllowed;
    private boolean mAreRefreshTokensLoaded = true;

    @Override
    public void addObserver(Observer observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> mObservers.add(observer));
    }

    @Override
    public void removeObserver(Observer observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> mObservers.remove(observer));
    }

    @Override
    public boolean hasPrimaryAccount() {
        return mPrimaryAccount != null;
    }

    @Override
    public @Nullable CoreAccountInfo getPrimaryAccountInfo() {
        return mPrimaryAccount;
    }

    @Override
    public @Nullable AccountInfo findExtendedAccountInfoByEmailAddress(String email) {
        for (var accountInfo : mExtendedAccountInfos.values()) {
            if (email.equals(accountInfo.getEmail())) {
                return accountInfo;
            }
        }
        return null;
    }

    @Override
    public @Nullable AccountInfo findExtendedAccountInfoByAccountId(CoreAccountId accountId) {
        return mExtendedAccountInfos.get(accountId);
    }

    @Override
    public void refreshAccountInfoIfStale() {}

    @Override
    public boolean isClearPrimaryAccountAllowed() {
        return mIsClearPrimaryAccountAllowed;
    }

    @MainThread
    @Override
    public void invalidateAccessToken(String accessToken) {}

    @Override
    public List<AccountInfo> getExtendedAccountInfoForAccountsWithRefreshToken() {
        return new ArrayList<>(mExtendedAccountInfos.values());
    }

    @Override
    public boolean areRefreshTokensLoaded() {
        return mAreRefreshTokensLoaded;
    }

    /**
     * Sets the extended account info. This account info is returned on future calls to {@link
     * #findExtendedAccountInfoByEmailAddress(String)}.
     */
    public void addOrUpdateExtendedAccountInfo(AccountInfo accountInfo) {
        assert accountInfo != null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mExtendedAccountInfos.put(accountInfo.getId(), accountInfo);
                    if (mAreRefreshTokensLoaded) {
                        for (Observer observer : mObservers) {
                            observer.onRefreshTokenUpdatedForAccount(accountInfo);
                        }
                    }
                    if (!mIsOnExtendedAccountInfoUpdatedBlocked) {
                        for (Observer observer : mObservers) {
                            observer.onExtendedAccountInfoUpdated(accountInfo);
                        }
                    }
                });
    }

    /**
     * Sets the primary account. If `accountInfo` is null then primary account is cleared. At least
     * one of the current primary account and accountInfo must be non-null and the other must be
     * null.
     */
    public void setPrimaryAccount(@Nullable CoreAccountInfo accountInfo) {
        assert mPrimaryAccount != null || accountInfo != null;
        assert mPrimaryAccount == null || accountInfo == null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrimaryAccount = accountInfo;
                    for (Observer observer : mObservers) {
                        @PrimaryAccountChangeEvent.Type
                        int type =
                                mPrimaryAccount == null
                                        ? PrimaryAccountChangeEvent.Type.SET
                                        : PrimaryAccountChangeEvent.Type.CLEARED;
                        observer.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(type));
                    }
                });
    }

    /** Removes the account with the given account ID from the fake IdentityManager. */
    public void removeAccount(CoreAccountId accountId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var removedAccountInfo = mExtendedAccountInfos.remove(accountId);
                    if (mPrimaryAccount != null && mPrimaryAccount.getId().equals(accountId)) {
                        mPrimaryAccount = null;
                    }
                    if (mAreRefreshTokensLoaded) {
                        for (Observer observer : mObservers) {
                            observer.onRefreshTokenRemovedForAccount(accountId);
                        }
                    }
                    if (!mIsOnExtendedAccountInfoUpdatedBlocked && removedAccountInfo != null) {
                        for (Observer observer : mObservers) {
                            observer.onExtendedAccountInfoUpdated(removedAccountInfo);
                        }
                    }
                });
    }

    /** Removes all accounts from the fake IdentityManager. */
    public void removeAllAccounts() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<CoreAccountId> accountIds =
                            new ArrayList<>(mExtendedAccountInfos.keySet());
                    for (CoreAccountId accountId : accountIds) {
                        removeAccount(accountId);
                    }
                });
    }

    public void setAreRefreshTokensLoaded(boolean areRefreshTokensLoaded) {
        mAreRefreshTokensLoaded = areRefreshTokensLoaded;
        if (mAreRefreshTokensLoaded) {
            for (Observer observer : mObservers) {
                observer.onRefreshTokensLoaded();
            }
        }
    }

    public void setIsClearPrimaryAccountAllowed(boolean isAllowed) {
        mIsClearPrimaryAccountAllowed = isAllowed;
    }

    public int getObserverCount() {
        return mObservers.size();
    }

    public void blockExtendedAccountInfoUpdate() {
        mIsOnExtendedAccountInfoUpdatedBlocked = true;
    }
}
