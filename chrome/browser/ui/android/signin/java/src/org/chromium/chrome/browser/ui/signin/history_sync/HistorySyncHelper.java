// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** A helper object that provides history sync opt-in related utilities. */
public class HistorySyncHelper {
    @Nullable private static HistorySyncHelper sHistorySyncHelperForTest;
    private final SyncService mSyncService;

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

    public boolean shouldSuppressHistorySync() {
        return didAlreadyOptIn()
                || isHistorySyncDisabledByCustodian()
                || isHistorySyncDisabledByPolicy();
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

    private void recordUserAlreadyOptedIn(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.AlreadyOptedIn", accessPoint, SigninAccessPoint.MAX);
    }

    private void recordHistorySyncSkipped(@SigninAccessPoint int accessPoint) {
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Skipped", accessPoint, SigninAccessPoint.MAX);
    }
}
