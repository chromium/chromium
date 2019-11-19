// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.ProfileDataSource;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * ProfileDataSource used for testing. Use {@link #setProfileData} to specify the data to be
 * returned by {@link #getProfileDataForAccount}.
 */
public class FakeProfileDataSource implements ProfileDataSource {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Map<String, ProfileData> mProfileDataMap = new HashMap<>();

    public FakeProfileDataSource() {}

    @Override
    public Map<String, ProfileData> getProfileDataMap() {
        ThreadUtils.assertOnUiThread();
        return Collections.unmodifiableMap(mProfileDataMap);
    }

    @Override
    public @Nullable ProfileData getProfileDataForAccount(String accountId) {
        ThreadUtils.assertOnUiThread();
        return mProfileDataMap.get(accountId);
    }

    @Override
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.removeObserver(observer);
        assert success : "Can't find observer";
    }

    /**
     * Sets or removes ProfileData for a single account. Will notify the observers.
     * @param profileData ProfileData to set or null to remove ProfileData from the account.
     */
    public void setProfileData(String accountId, @Nullable ProfileData profileData) {
        ThreadUtils.assertOnUiThread();
        if (profileData == null) {
            mProfileDataMap.remove(accountId);
        } else {
            assert accountId.equals(profileData.getAccountName());
            mProfileDataMap.put(accountId, profileData);
        }
        fireOnProfileDataUpdatedNotification(accountId);
    }

    private void fireOnProfileDataUpdatedNotification(String accountId) {
        for (Observer observer : mObservers) {
            observer.onProfileDataUpdated(accountId);
        }
    }
}
