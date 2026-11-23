// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.List;

/** Input events dispatched to {@link FullscreenSigninReducer}. */
@NullMarked
sealed interface FullscreenSigninEvent {
    /** Native initialization has completed. */
    final class NativeReady implements FullscreenSigninEvent {}

    /**
     * The {@link org.chromium.chrome.browser.signin.services.ProfileDataCache} was created. Carries
     * the account IDs if the accounts had already finished loading by then, since no {@link
     * AccountsUpdated} is sent for them.
     */
    final class ProfileDataCacheCreated implements FullscreenSigninEvent {
        private final @Nullable List<CoreAccountId> mAccountIds;

        ProfileDataCacheCreated(@Nullable List<CoreAccountId> accountIds) {
            mAccountIds = accountIds == null ? null : List.copyOf(accountIds);
        }

        /** The IDs of the already-loaded accounts, or null if they're still loading. */
        @Nullable List<CoreAccountId> getAccountIds() {
            return mAccountIds;
        }
    }

    /** Policy loading has completed. */
    final class PoliciesLoaded implements FullscreenSigninEvent {}

    /** The host's child account status is available. */
    final class ChildAccountStatusLoaded implements FullscreenSigninEvent {}

    /**
     * The account IDs, sent when the accounts finish loading after the cache is created and again
     * whenever the accounts or their info change.
     */
    final class AccountsUpdated implements FullscreenSigninEvent {
        private final List<CoreAccountId> mAccountIds;

        AccountsUpdated(List<CoreAccountId> accountIds) {
            mAccountIds = List.copyOf(accountIds);
        }

        List<CoreAccountId> getAccountIds() {
            return mAccountIds;
        }
    }
}
