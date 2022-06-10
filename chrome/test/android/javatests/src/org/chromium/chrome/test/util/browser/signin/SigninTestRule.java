// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * This test rule mocks AccountManagerFacade and manages sign-in/sign-out.
 *
 * TODO(crbug.com/1334286): Migrate usage of {@link AccountManagerTestRule} that need native to this
 * rule, then inline the methods that call native.
 *
 * Calling the sign-in functions will invoke native code, therefore this should only be used in
 * on-device tests. In Robolectric tests, use the {@link AccountManagerTestRule} instead as a simple
 * AccountManagerFacade mock.
 */
public class SigninTestRule extends AccountManagerTestRule {
    /**
     * Waits for the AccountTrackerService to seed system accounts.
     */
    @Override
    public void waitForSeeding() {
        super.waitForSeeding();
    }

    /**
     * Adds an account and seed it in native code.
     */
    @Override
    public CoreAccountInfo addAccountAndWaitForSeeding(String accountName) {
        return super.addAccountAndWaitForSeeding(accountName);
    }

    /**
     * Removes an account and seed it in native code.
     */
    @Override
    public void removeAccountAndWaitForSeeding(String accountEmail) {
        super.removeAccountAndWaitForSeeding(accountEmail);
    }

    /**
     * Adds and signs in an account with the default name without sync consent.
     */
    @Override
    public CoreAccountInfo addTestAccountThenSignin() {
        return super.addTestAccountThenSignin();
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     */
    @Override
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync() {
        return super.addTestAccountThenSigninAndEnableSync();
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't
     *         start.
     */
    @Override
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync(
            @Nullable SyncService syncService) {
        return super.addTestAccountThenSigninAndEnableSync(syncService);
    }

    /**
     * Adds a child account, and waits for auto-signin to complete.
     */
    @Override
    public CoreAccountInfo addChildTestAccountThenWaitForSignin() {
        return super.addChildTestAccountThenWaitForSignin();
    }

    /**
     * Adds a child account, waits for auto-signin to complete, and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't
     *         start.
     */
    @Override
    public CoreAccountInfo addChildTestAccountThenEnableSync(@Nullable SyncService syncService) {
        return super.addChildTestAccountThenEnableSync(syncService);
    }

    /**
     * Adds and signs in an account with the default name and enables sync.
     *
     * @param syncService SyncService object to set up sync, if null, sync won't
     *         start.
     * @param isChild Whether this is a supervised child account.
     */
    @Override
    public CoreAccountInfo addTestAccountThenSigninAndEnableSync(
            @Nullable SyncService syncService, boolean isChild) {
        return super.addTestAccountThenSigninAndEnableSync(syncService, isChild);
    }

    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     */
    @Override
    public CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return super.getPrimaryAccount(consentLevel);
    }

    /**
     * Sign out from the current account.
     */
    @Override
    public void signOut() {
        super.signOut();
    }

    /**
     * Sign out from the current account, ignoring usual checks (suitable for eg. test teardown, but
     * not feature testing).
     */
    @Override
    public void forceSignOut() {
        super.forceSignOut();
    }
}
