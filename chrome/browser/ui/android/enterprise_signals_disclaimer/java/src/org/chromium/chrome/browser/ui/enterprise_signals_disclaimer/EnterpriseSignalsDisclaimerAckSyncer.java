// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.ArrayList;
import java.util.List;

/**
 * Observer that syncs the Enterprise Signals Disclaimer acknowledgment set when the accounts on the
 * device change.
 */
@NullMarked
public class EnterpriseSignalsDisclaimerAckSyncer implements IdentityManager.Observer {
    private static @Nullable EnterpriseSignalsDisclaimerAckSyncer sInstance;
    private final IdentityManager mIdentityManager;

    /**
     * Initializes the {@link EnterpriseSignalsDisclaimerAckSyncer}.
     *
     * @param profile The profile to use for the {@link EnterpriseSignalsDisclaimerAckSyncer}.
     */
    public static void initialize(Profile profile) {
        IdentityManager identityManager =
                assertNonNull(IdentityServicesProvider.get().getIdentityManager(profile));
        if (sInstance == null) {
            sInstance = new EnterpriseSignalsDisclaimerAckSyncer(identityManager);
        }
    }

    private EnterpriseSignalsDisclaimerAckSyncer(IdentityManager identityManager) {
        mIdentityManager = identityManager;
        mIdentityManager.addObserver(this);
        syncAcknowledgmentSet();
    }

    /** Implements {@link IdentityManager.Observer} */
    @Override
    public void onRefreshTokensLoaded() {
        syncAcknowledgmentSet();
    }

    /** Implements {@link IdentityManager.Observer} */
    @Override
    public void onRefreshTokenRemovedForAccount(CoreAccountId accountId) {
        syncAcknowledgmentSet();
    }

    private void syncAcknowledgmentSet() {
        if (!mIdentityManager.areRefreshTokensLoaded()) {
            return;
        }

        final List<AccountInfo> accountsOnDevice =
                mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken();
        List<GaiaId> gaiaIdsOnDevice = new ArrayList<>(accountsOnDevice.size());
        for (AccountInfo account : accountsOnDevice) {
            gaiaIdsOnDevice.add(account.getGaiaId());
        }
        // This function will clear the Enterprise Signals Disclaimer acknowledgment for
        // accounts that are not on the device anymore.
        EnterpriseSignalsDisclaimerBridge.removeUnknownAccounts(gaiaIdsOnDevice);
    }

    public static void resetForTesting() {
        if (sInstance != null) {
            sInstance.mIdentityManager.removeObserver(sInstance);
            sInstance = null;
        }
    }
}
