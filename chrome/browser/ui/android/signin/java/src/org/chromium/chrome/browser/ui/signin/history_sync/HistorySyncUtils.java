// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** Provides history sync opt-in related utility functions. */
// TODO(b/41493766): Consider adding a dedicated unit test class.
public final class HistorySyncUtils {
    /** Whether the user has already opted in to sync their history and tabs. */
    public static boolean didAlreadyOptIn(Profile profile) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assert syncService != null;
        return syncService
                .getSelectedTypes()
                .containsAll(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));
    }

    /** Whether history sync is disabled by enterprise policy. */
    public static boolean isHistorySyncDisabledByPolicy(Profile profile) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assert syncService != null;
            return syncService.isSyncDisabledByEnterprisePolicy()
                    || syncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)
                    || syncService.isTypeManagedByPolicy(UserSelectableType.TABS);
    }

    /** Whether history sync is disabled by the user's custodian. */
    public static boolean isHistorySyncDisabledByCustodian(Profile profile) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assert syncService != null;
            return syncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)
                    || syncService.isTypeManagedByCustodian(UserSelectableType.TABS);
    }

    /**
     * Records the correct metric for the history sync opt-in not being shown, depending on the
     * reason.
     */
    public static void recordHistorySyncNotShown(
            Profile profile, @SigninAccessPoint int accessPoint) {
        if (didAlreadyOptIn(profile)) {
            recordUserAlreadyOptedIn(accessPoint);
            return;
        }
        recordHistorySyncSkipped(accessPoint);
    }

    private static void recordUserAlreadyOptedIn(@SigninAccessPoint int accessPoint) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.AlreadyOptedIn", accessPoint, SigninAccessPoint.MAX);
    }

    private static void recordHistorySyncSkipped(@SigninAccessPoint int accessPoint) {
        assert ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS);
        RecordHistogram.recordEnumeratedHistogram(
                "Signin.HistorySyncOptIn.Skipped", accessPoint, SigninAccessPoint.MAX);
    }
}
