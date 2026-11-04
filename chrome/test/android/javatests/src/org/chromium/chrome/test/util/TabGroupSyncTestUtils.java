// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

/** Test utilities for {@link TabGroupSyncService}. */
@NullMarked
public final class TabGroupSyncTestUtils {
    private TabGroupSyncTestUtils() {}

    /**
     * Removes all saved tab groups from the regular profile's {@link TabGroupSyncService}.
     *
     * <p>Closing a window hides its tab groups instead of deleting them, so they stay in the
     * profile-keyed {@link TabGroupSyncService} for the rest of the process, e.g. later tests in a
     * batch. Does nothing if native is not initialized, or the profile or service were replaced
     * with mocks.
     */
    public static void removeAllSavedGroups() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!ProfileManager.isInitialized()) return;
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    // Skip mocked profiles, e.g. from
                    // ProfileManager.setLastUsedProfileForTesting().
                    if (!profile.isNativeInitialized()) return;
                    TabGroupSyncService service = TabGroupSyncServiceFactory.getForProfile(profile);
                    if (service == null || Mockito.mockingDetails(service).isMock()) return;
                    for (String syncId : service.getAllGroupIds()) {
                        service.removeGroup(syncId);
                    }
                });
    }
}
