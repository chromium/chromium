// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CreateProfileDataCache;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.AccountsUpdated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.ProfileDataCacheCreated;
import org.chromium.components.signin.base.AccountInfo;

import java.util.ArrayList;
import java.util.List;

/**
 * Pure state machine for the fullscreen sign-in screen: {@code reduce(state, event) -> (state,
 * effects)}.
 */
@NullMarked
final class FullscreenSigninReducer {
    /** The new state and the effects to run, in order. */
    static final class Result {
        private final FullscreenSigninState mState;
        private final List<FullscreenSigninEffect> mEffects;

        Result(FullscreenSigninState state, List<FullscreenSigninEffect> effects) {
            mState = state;
            mEffects = List.copyOf(effects);
        }

        FullscreenSigninState getState() {
            return mState;
        }

        List<FullscreenSigninEffect> getEffects() {
            return mEffects;
        }
    }

    private FullscreenSigninReducer() {}

    /** Returns the state and effects after {@code event}. */
    static Result reduce(FullscreenSigninState state, FullscreenSigninEvent event) {
        FullscreenSigninState.Builder builder = state.buildUpon();
        List<FullscreenSigninEffect> effects = new ArrayList<>();
        switch (event) {
            case NativeReady _ -> {
                // Native initialization only completes once, so a repeated event changes nothing.
                if (state.isNativeLoaded()) return new Result(state, List.of());
                builder.setNativeLoaded();
                // Record the native-initialized histogram before creating the cache, as the
                // legacy mediator does, so the timing isn't inflated.
                effects.add(RecordNativeInitialized.INSTANCE);
                effects.add(CreateProfileDataCache.INSTANCE);
            }
            case ProfileDataCacheCreated e -> {
                List<AccountInfo> accounts = e.getAccounts();
                if (accounts != null) onAccountsLoaded(builder, accounts);
            }
            case AccountsUpdated e -> onAccountsLoaded(builder, e.getAccounts());
        }
        return new Result(builder.build(), effects);
    }

    /** Updates the state for a newly loaded account list. */
    private static void onAccountsLoaded(
            FullscreenSigninState.Builder builder, List<AccountInfo> accounts) {
        builder.setSelectedAccount(selectAccount(accounts));
    }

    /** Selects the first account, or none if there are no accounts. */
    private static @Nullable AccountInfo selectAccount(List<AccountInfo> accounts) {
        // TODO(crbug.com/579875669): As in the legacy mediator, select the preselected (config)
        // or preferred account when there is one, and keep the current selection when the
        // accounts change.
        return accounts.isEmpty() ? null : accounts.get(0);
    }
}
