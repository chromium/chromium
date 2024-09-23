// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import androidx.annotation.Nullable;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.time.Duration;
import java.util.Set;

/** A helper object that provides history sync opt-in related utilities. */
public class HistorySyncHelper {
    private static final int MAX_SUCCESSIVE_DECLINES = 2;
    private static final long MIN_DAYS_SINCE_LAST_DECLINE = 14;
    @Nullable private static HistorySyncHelper sHistorySyncHelperForTest;
    private final SyncService mSyncService;
    private final PrefService mPrefService;

    public static HistorySyncHelper getForProfile(Profile profile) {
        if (sHistorySyncHelperForTest != null) {
            return sHistorySyncHelperForTest;
        }
        return new HistorySyncHelper(profile);
    }

    public static void setInstanceForTesting(HistorySyncHelper historySyncHelper) {
        sHistorySyncHelperForTest = historySyncHelper;
    }

    private HistorySyncHelper(Profile profile) {
        mSyncService = SyncServiceFactory.getForProfile(profile);
        mPrefService = UserPrefs.get(profile);
    }

    /** Whether the user has already opted in to sync their history and tabs. */
    public boolean didAlreadyOptIn() {
        return mSyncService
                .getSelectedTypes()
                .containsAll(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));
    }

    /** Whether history sync is disabled by enterprise policy. */
    public boolean isHistorySyncDisabledByPolicy() {
        return mSyncService.isSyncDisabledByEnterprisePolicy()
                || mSyncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)
                || mSyncService.isTypeManagedByPolicy(UserSelectableType.TABS);
    }

    /** Whether history sync is disabled by the user's custodian. */
    public boolean isHistorySyncDisabledByCustodian() {
        return mSyncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)
                || mSyncService.isTypeManagedByCustodian(UserSelectableType.TABS);
    }

    /** Whether the history sync prompt should be suppressed. */
    public boolean shouldSuppressHistorySync() {
        return didAlreadyOptIn()
                || isHistorySyncDisabledByCustodian()
                || isHistorySyncDisabledByPolicy();
    }

    /** Whether history sync is often declined. */
    public boolean isDeclinedOften() {
        if (mPrefService.getInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT)
                >= MAX_SUCCESSIVE_DECLINES) {
            // The user has declined two or more opt-ins in a row.
            return true;
        }
        final long lastDecline = mPrefService.getLong(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP);
        // The user has declined in the past two weeks.
        return Duration.ofMillis(TimeUtils.currentTimeMillis() - lastDecline).toDays()
                < MIN_DAYS_SINCE_LAST_DECLINE;
    }

    /**
     * Records the correct metric for the history sync opt-in not being shown, depending on the
     * reason.
     */
    public void recordHistorySyncNotShown(@SigninAccessPoint int accessPoint) {
        if (didAlreadyOptIn()) {
            recordUserAlreadyOptedIn(accessPoint);
            return;
        }
        recordHistorySyncSkipped(accessPoint);
    }

    public void recordHistorySyncDeclinedPrefs() {
        int declineCount = mPrefService.getInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT);
        mPrefService.setInteger(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT, declineCount + 1);
        mPrefService.setLong(
                Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP, TimeUtils.currentTimeMillis());
    }

    public void clearHistorySyncDeclinedPrefs() {
        mPrefService.clearPref(Pref.HISTORY_SYNC_LAST_DECLINED_TIMESTAMP);
        mPrefService.clearPref(Pref.HISTORY_SYNC_SUCCESSIVE_DECLINE_COUNT);
    }

    private void recordUserAlreadyOptedIn(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.AlreadyOptedIn", accessPoint, SigninAccessPoint.MAX);
    }

    private void recordHistorySyncSkipped(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Skipped", accessPoint, SigninAccessPoint.MAX);
    }
}
