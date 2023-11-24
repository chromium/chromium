// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.signin.Tribool;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Utility for tracking how much time it will get to have {@link AccountCapabilities} ready for the
 * primary account.
 */
class AccountCapabilitiesLatencyTracker implements IdentityManager.Observer {
    private static final String USER_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.UserVisibleLatency";
    private static final String FETCH_LATENCY_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.FetchLatency";
    private static final String IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME =
            "Signin.AccountCapabilities.ImmediatelyAvailable";

    private final long mCreated = SystemClock.elapsedRealtime();
    private final AccountInfo mPrimaryAccount;
    private final IdentityManager mIdentityManager;

    private AccountCapabilitiesLatencyTracker(
            IdentityManager identityManager, AccountInfo primaryAccount) {
        this.mPrimaryAccount = primaryAccount;
        this.mIdentityManager = identityManager;
    }

    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        if (!mPrimaryAccount.getId().equals(accountInfo.getId())) {
            return;
        }

        if (hasCapabilities(accountInfo)) {
            recordFetchLatency();
            mIdentityManager.removeObserver(this);
        }
    }

    /** Returns true iff {@param accountInfo} contains an example capability. */
    private static boolean hasCapabilities(AccountInfo accountInfo) {
        AccountCapabilities capabilities = accountInfo.getAccountCapabilities();
        if (capabilities == null) {
            return false;
        }
        // TODO(b/309953195): Replace with target capability. Current check is an approximation.
        return capabilities.isSubjectToParentalControls() != Tribool.UNKNOWN;
    }

    /** Tracks the availability latency of {@link AccountCapabilities} for the primary account. */
    public static void trackAccountCapabilitiesFetchLatency(
            IdentityManager identityManager, CoreAccountInfo primaryAccount) {
        AccountInfo accountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(primaryAccount.getEmail());
        if (accountInfo != null && hasCapabilities(accountInfo)) {
            recordImmediateAvailability();
            return;
        }

        recordNoImmediateAvailability();
        identityManager.addObserver(
                new AccountCapabilitiesLatencyTracker(identityManager, accountInfo));
    }

    private static void recordImmediateAvailability() {
        RecordHistogram.recordTimesHistogram(USER_LATENCY_HISTOGRAM_NAME, 0);
        RecordHistogram.recordBooleanHistogram(IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME, true);
    }

    private static void recordNoImmediateAvailability() {
        RecordHistogram.recordBooleanHistogram(IMMEDIATELY_AVAILABLE_HISTOGRAM_NAME, false);
    }

    private void recordFetchLatency() {
        long latency = SystemClock.elapsedRealtime() - mCreated;
        RecordHistogram.recordTimesHistogram(USER_LATENCY_HISTOGRAM_NAME, latency);
        RecordHistogram.recordTimesHistogram(FETCH_LATENCY_HISTOGRAM_NAME, latency);
    }
}
