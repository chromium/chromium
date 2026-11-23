// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.CreateProfileDataCache;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.NotifyInitialLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordLoadCompleted;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.AccountsUpdated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.ChildAccountStatusLoaded;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.PoliciesLoaded;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.ProfileDataCacheCreated;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninState.Phase;
import org.chromium.google_apis.gaia.CoreAccountId;

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
        @FullscreenSigninLoadPoint int loadPoint;
        switch (event) {
            case NativeReady _ -> {
                assert !state.isNativeLoaded();
                builder.setNativeLoaded();
                // Record the native-initialized histogram before creating the cache, as the
                // legacy mediator does, so the timing isn't inflated.
                effects.add(RecordNativeInitialized.INSTANCE);
                effects.add(CreateProfileDataCache.INSTANCE);
                // The accounts can't have loaded yet (they come from the cache), so the gate
                // can't open here.
                return new Result(builder.build(), effects);
            }
            case ProfileDataCacheCreated e -> {
                List<CoreAccountId> accountIds = e.getAccountIds();
                if (accountIds != null) onAccountsLoaded(builder, accountIds);
                // The cache is created as soon as native is ready, so accounts that were already
                // loaded by then make native the slowest load point.
                loadPoint = FullscreenSigninLoadPoint.NATIVE_INITIALIZATION;
            }
            case PoliciesLoaded _ -> {
                assert !state.arePoliciesLoaded();
                builder.setPoliciesLoaded();
                loadPoint = FullscreenSigninLoadPoint.POLICY_LOAD;
            }
            case ChildAccountStatusLoaded _ -> {
                assert !state.isChildStatusLoaded();
                builder.setChildStatusLoaded();
                loadPoint = FullscreenSigninLoadPoint.CHILD_STATUS_LOAD;
            }
            case AccountsUpdated e -> {
                onAccountsLoaded(builder, e.getAccountIds());
                loadPoint = FullscreenSigninLoadPoint.ACCOUNT_FETCHING;
            }
        }

        FullscreenSigninState nextState = builder.build();
        if (nextState.getPhase() == Phase.LOADING && nextState.isInitialLoadComplete()) {
            nextState = nextState.buildUpon().setPhase(Phase.READY).build();
            // Notify the delegate before recording the load histograms: they measure the time
            // until this point, so they include showing the loaded screen.
            effects.add(NotifyInitialLoadCompleted.INSTANCE);
            effects.add(new RecordLoadCompleted(loadPoint));
        }
        return new Result(nextState, effects);
    }

    /** Updates the state for a newly loaded account list. */
    private static void onAccountsLoaded(
            FullscreenSigninState.Builder builder, List<CoreAccountId> accountIds) {
        builder.setAccountsLoaded().setSelectedAccountId(selectAccount(accountIds));
    }

    /** Selects the first account, or none if there are no accounts. */
    private static @Nullable CoreAccountId selectAccount(List<CoreAccountId> accountIds) {
        // TODO(crbug.com/579875669): As in the legacy mediator, select the preselected (config)
        // or preferred account when there is one, and keep the current selection when the
        // accounts change.
        return accountIds.isEmpty() ? null : accountIds.get(0);
    }
}
