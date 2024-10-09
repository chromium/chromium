// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.signin;

import org.hamcrest.Matchers;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * Base class for defining methods for signing in an live account for testing. The correct version
 * of LiveSigninTestUtilImpl will be determined at compile time via build rules.
 */
public class LiveSigninTestUtil {
    private static final String TAG = "LiveSigninTestUtil";
    private static final long MAX_TIME_TO_POLL_MS = 10000L;
    private static LiveSigninTestUtil sInstance;

    public static LiveSigninTestUtil getInstance() {
        if (sInstance == null) {
            sInstance = ServiceLoaderUtil.maybeCreate(LiveSigninTestUtil.class);
            if (sInstance == null) {
                sInstance = new LiveSigninTestUtil();
            }
        }
        return sInstance;
    }

    /** Add a live account to the device and signs in for testing, but does not enable Sync. */
    public void addAccountWithPasswordThenSignin(String accountName, String password) {
        CoreAccountInfo coreAccountInfo = addAccountWithPassword(accountName, password);
        SigninTestUtil.signin(coreAccountInfo);
        Log.i(TAG, "Finished signin for account %s", coreAccountInfo.toString());
    }

    /** Add a live account to the device and signs in for testing, and enables Sync-the-feature. */
    public void addAccountWithPasswordThenSigninAndEnableSync(String accountName, String password) {
        CoreAccountInfo coreAccountInfo = addAccountWithPassword(accountName, password);
        SigninTestUtil.signinAndEnableSync(
                coreAccountInfo, SyncTestUtil.getSyncServiceForLastUsedProfile());
        Log.i(TAG, "Finished signin and enabling sync for account %s", coreAccountInfo.toString());
    }

    private CoreAccountInfo addAccountWithPassword(String accountName, String password) {
        if (addAccountWithPasswordImpl(accountName, password)) {
            Log.i(TAG, "Added account %s to device successfully.", accountName);
        } else {
            Log.e(TAG, "Failed to add account %s to device.", accountName);
        }
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            findAccountByEmailAddress(accountName), Matchers.notNullValue());
                },
                MAX_TIME_TO_POLL_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CoreAccountInfo coreAccountInfo =
                ThreadUtils.runOnUiThreadBlocking(() -> findAccountByEmailAddress(accountName));
        return coreAccountInfo;
    }

    protected boolean addAccountWithPasswordImpl(String accountName, String password) {
        throw new UnsupportedOperationException();
    }

    private CoreAccountInfo findAccountByEmailAddress(String email) {
        return IdentityServicesProvider.get()
                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                .findExtendedAccountInfoByEmailAddress(email);
    }
}
