// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.history_sync;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/** Provides history sync opt-in related utility functions. */
public final class HistorySyncUtils {
    /** Whether the user has already opted in to sync their history and tabs. */
    public static boolean didAlreadyOptIn(Profile profile) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            SyncService syncService = SyncServiceFactory.getForProfile(profile);
            assert syncService != null;
            return syncService
                    .getSelectedTypes()
                    .containsAll(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS));
        }
        final IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        return identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
    }

    /** Whether history sync is disabled by enterprise policy. */
    public static boolean isHistorySyncDisabledByPolicy(Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assert syncService != null;
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return syncService.isSyncDisabledByEnterprisePolicy()
                    || syncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)
                    || syncService.isTypeManagedByPolicy(UserSelectableType.TABS);
        }
        return false;
    }

    /** Whether history sync is disabled by the user's custodian. */
    public static boolean isHistorySyncDisabledByCustodian(Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        assert syncService != null;
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return syncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)
                    || syncService.isTypeManagedByCustodian(UserSelectableType.TABS);
        }
        return false;
    }
}
