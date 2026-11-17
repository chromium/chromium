// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;

import java.util.List;

/** Input events dispatched to {@link FullscreenSigninReducer}. */
@NullMarked
sealed interface FullscreenSigninEvent {
    /** Native initialization has completed. */
    final class NativeReady implements FullscreenSigninEvent {}

    /**
     * The {@link org.chromium.chrome.browser.signin.services.ProfileDataCache} was created. Carries
     * the accounts if they had already finished loading by then, since no {@link AccountsUpdated}
     * is sent for them.
     */
    final class ProfileDataCacheCreated implements FullscreenSigninEvent {
        private final @Nullable List<AccountInfo> mAccounts;

        ProfileDataCacheCreated(@Nullable List<AccountInfo> accounts) {
            mAccounts = accounts == null ? null : List.copyOf(accounts);
        }

        /** The already-loaded accounts, or null if they're still loading. */
        @Nullable List<AccountInfo> getAccounts() {
            return mAccounts;
        }
    }

    /**
     * The account list, sent when it finishes loading after the cache is created and again whenever
     * the accounts or their info change.
     */
    final class AccountsUpdated implements FullscreenSigninEvent {
        private final List<AccountInfo> mAccounts;

        AccountsUpdated(List<AccountInfo> accounts) {
            mAccounts = List.copyOf(accounts);
        }

        List<AccountInfo> getAccounts() {
            return mAccounts;
        }
    }
}
