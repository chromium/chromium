// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Observable source of profile data for accounts on device. Must be used from UI thread only.
 */
public interface ProfileDataSource {
    /**
     * Immutable holder for profile data.
     */
    class ProfileData {
        private final String mAccountEmail;
        private final @Nullable Bitmap mAvatar;
        private final @Nullable String mFullName;
        private final @Nullable String mGivenName;

        public ProfileData(String accountEmail, @Nullable Bitmap avatar, @Nullable String fullName,
                @Nullable String givenName) {
            assert accountEmail != null;
            this.mAccountEmail = accountEmail;
            this.mAvatar = avatar;
            this.mFullName = fullName;
            this.mGivenName = givenName;
        }

        /**
         * Gets the account email address.
         * @return the account email.
         */
        public String getAccountEmail() {
            return mAccountEmail;
        }

        /**
         * Gets the avatar.
         * @return the avatar if it is known, otherwise returns null.
         */
        public @Nullable Bitmap getAvatar() {
            return mAvatar;
        }

        /**
         * Gets the full name (e.g., John Doe).
         * @return the full name if it is known, otherwise returns null.
         */
        public @Nullable String getFullName() {
            return mFullName;
        }

        /**
         * Gets the given name (e.g., John).
         * @return the given name if it is known, otherwise returns null.
         */
        public @Nullable String getGivenName() {
            return mGivenName;
        }
    }

    /**
     * Observer to get notifications about changes in profile data.
     */
    interface Observer {
        /**
         * Notifies that an account's profile data has been updated.
         */
        void onProfileDataUpdated(ProfileData profileData);

        /**
         * Removes the profile data of a given accountEmail.
         */
        void removeProfileData(String accountEmail);
    }

    /**
     * Gets ProfileData for all accounts. There must be at least one active observer when this
     * method is invoked (see {@link #addObserver}).
     * @return unmodifiable map of ProfileData for all accounts (keyed by account name).
     * TODO(crbug/1136452): Remove this method after removing the downstream implementation.
     */
    default Map<String, ProfileData> getProfileDataMap() {
        return null;
    }

    /**
     * Gets ProfileData for single account. There must be at least one active observer when this
     * method is invoked (see {@link #addObserver}).
     * @param accountEmail account email to get ProfileData for.
     * @return ProfileData if there's any profile data for this account name, null otherwise.
     */
    @Nullable
    ProfileData getProfileDataForAccount(String accountEmail);

    /**
     * Adds an observer to get notified about changes to profile data.
     * @param observer observer that should be notified when new profile data is available.
     */
    void addObserver(Observer observer);

    /**
     * Removes an observer previously added by {@link #addObserver}.
     * @param observer observer to remove.
     */
    void removeObserver(Observer observer);
}
