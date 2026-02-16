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
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Fake implementation of {@link IdentityManager} for testing. */
@NullMarked
public class FakeIdentityManager implements IdentityManager {
    private final List<Observer> mObservers = new ArrayList<>();
    private final Map<CoreAccountId, AccountInfo> mExtendedAccountInfos = new HashMap<>();
    private @Nullable CoreAccountInfo mPrimaryAccount;
    private boolean mIsOnExtendedAccountInfoUpdatedBlocked;
    private boolean mIsClearPrimaryAccountAllowed;

    @Override
    public void addObserver(Observer observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> mObservers.add(observer));
    }

    @Override
    public void removeObserver(Observer observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> mObservers.remove(observer));
    }

    @Override
    public boolean hasPrimaryAccount(@ConsentLevel int consentLevel) {
        return mPrimaryAccount != null;
    }

    @Override
    public @Nullable CoreAccountInfo getPrimaryAccountInfo(@ConsentLevel int consentLevel) {
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

    /**
     * Sets the extended account info. This account info is returned on future calls to {@link
     * #findExtendedAccountInfoByEmailAddress(String)}.
     */
    public void addOrUpdateExtendedAccountInfo(AccountInfo accountInfo) {
        assert accountInfo != null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mExtendedAccountInfos.put(accountInfo.getId(), accountInfo);
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
                        observer.onPrimaryAccountChanged(
                                new PrimaryAccountChangeEvent(
                                        type, PrimaryAccountChangeEvent.Type.NONE));
                    }
                });
    }

    /** Removes the account with the given account ID from the fake IdentityManager. */
    public void removeAccount(CoreAccountId accountId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mExtendedAccountInfos.remove(accountId);
                    if (mPrimaryAccount != null && mPrimaryAccount.getId().equals(accountId)) {
                        mPrimaryAccount = null;
                    }
                });
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
