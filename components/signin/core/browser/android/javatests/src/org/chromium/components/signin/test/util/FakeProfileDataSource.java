// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.ProfileDataSource;

import java.util.HashMap;
import java.util.Map;

/**
 * ProfileDataSource used for testing. Use {@link #addProfileData} to specify the data to be
 * returned by {@link #getProfileDataForAccount}.
 */
public class FakeProfileDataSource implements ProfileDataSource {
    protected final ObserverList<Observer> mObservers = new ObserverList<>();
    protected final Map<String, ProfileData> mProfileDataMap = new HashMap<>();

    public FakeProfileDataSource() {}

    @Override
    public @Nullable ProfileData getProfileDataForAccount(String accountEmail) {
        ThreadUtils.assertOnUiThread();
        return mProfileDataMap.get(accountEmail);
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
     * Adds a {@link ProfileData} to the FakeProfileDataSource.
     * If the account email of the {@link ProfileData} already exists, replace the old
     * {@link ProfileData} with the given one.
     */
    public void addProfileData(ProfileData profileData) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileDataMap.put(profileData.getAccountEmail(), profileData);
            for (Observer observer : mObservers) {
                observer.onProfileDataUpdated(profileData);
            }
        });
    }
}
