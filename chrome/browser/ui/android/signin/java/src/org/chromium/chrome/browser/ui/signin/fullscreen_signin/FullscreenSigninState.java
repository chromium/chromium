// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Immutable state of the fullscreen sign-in screen. */
@NullMarked
final class FullscreenSigninState {
    @IntDef({Phase.LOADING, Phase.FINALIZING_LOAD, Phase.READY})
    @Retention(RetentionPolicy.SOURCE)
    @interface Phase {
        /** Waiting for the load inputs. */
        int LOADING = 0;

        /** All load inputs have arrived; checking what's needed to finish loading. */
        int FINALIZING_LOAD = 1;

        /** Loading has finished. */
        int READY = 2;
    }

    private final @Phase int mPhase;
    private final boolean mConstructing;
    private final boolean mNativeLoaded;
    private final boolean mPoliciesLoaded;
    private final boolean mChildStatusLoaded;
    private final boolean mAccountsLoaded;
    private final @FullscreenSigninLoadPoint int mSlowestLoadPoint;
    private final @Nullable CoreAccountId mSelectedAccountId;

    /**
     * Creates the state before any input has arrived. Construction is in progress until {@link
     * FullscreenSigninEvent.ConstructionFinished}.
     */
    FullscreenSigninState() {
        mPhase = Phase.LOADING;
        mConstructing = true;
        mNativeLoaded = false;
        mPoliciesLoaded = false;
        mChildStatusLoaded = false;
        mAccountsLoaded = false;
        mSlowestLoadPoint = FullscreenSigninLoadPoint.NATIVE_INITIALIZATION;
        mSelectedAccountId = null;
    }

    private FullscreenSigninState(Builder builder) {
        mPhase = builder.mPhase;
        mConstructing = builder.mConstructing;
        mNativeLoaded = builder.mNativeLoaded;
        mPoliciesLoaded = builder.mPoliciesLoaded;
        mChildStatusLoaded = builder.mChildStatusLoaded;
        mAccountsLoaded = builder.mAccountsLoaded;
        mSlowestLoadPoint = builder.mSlowestLoadPoint;
        mSelectedAccountId = builder.mSelectedAccountId;
    }

    @Phase
    int getPhase() {
        return mPhase;
    }

    /** Whether the mediator is still being created. */
    boolean isConstructing() {
        return mConstructing;
    }

    /** Whether native initialization has completed. */
    boolean isNativeLoaded() {
        return mNativeLoaded;
    }

    boolean arePoliciesLoaded() {
        return mPoliciesLoaded;
    }

    boolean isChildStatusLoaded() {
        return mChildStatusLoaded;
    }

    /** The selected account's ID, or null if the accounts haven't loaded yet or there are none. */
    @Nullable CoreAccountId getSelectedAccountId() {
        return mSelectedAccountId;
    }

    /** The load input that arrived last. Set when loading enters {@link Phase#FINALIZING_LOAD}. */
    @FullscreenSigninLoadPoint
    int getSlowestLoadPoint() {
        assert mPhase == Phase.FINALIZING_LOAD;
        return mSlowestLoadPoint;
    }

    /** Whether all four initial-load inputs have arrived. */
    boolean isInitialLoadComplete() {
        return mNativeLoaded && mPoliciesLoaded && mChildStatusLoaded && mAccountsLoaded;
    }

    Builder buildUpon() {
        return new Builder(this);
    }

    static final class Builder {
        private @Phase int mPhase;
        private boolean mConstructing;
        private boolean mNativeLoaded;
        private boolean mPoliciesLoaded;
        private boolean mChildStatusLoaded;
        private boolean mAccountsLoaded;
        private @FullscreenSigninLoadPoint int mSlowestLoadPoint;
        private @Nullable CoreAccountId mSelectedAccountId;

        private Builder(FullscreenSigninState state) {
            mPhase = state.mPhase;
            mConstructing = state.mConstructing;
            mNativeLoaded = state.mNativeLoaded;
            mPoliciesLoaded = state.mPoliciesLoaded;
            mChildStatusLoaded = state.mChildStatusLoaded;
            mAccountsLoaded = state.mAccountsLoaded;
            mSlowestLoadPoint = state.mSlowestLoadPoint;
            mSelectedAccountId = state.mSelectedAccountId;
        }

        Builder setPhase(@Phase int phase) {
            mPhase = phase;
            return this;
        }

        Builder setConstructionFinished() {
            mConstructing = false;
            return this;
        }

        Builder setNativeLoaded() {
            mNativeLoaded = true;
            return this;
        }

        Builder setPoliciesLoaded() {
            mPoliciesLoaded = true;
            return this;
        }

        Builder setChildStatusLoaded() {
            mChildStatusLoaded = true;
            return this;
        }

        Builder setAccountsLoaded() {
            mAccountsLoaded = true;
            return this;
        }

        Builder setSlowestLoadPoint(@FullscreenSigninLoadPoint int slowestLoadPoint) {
            mSlowestLoadPoint = slowestLoadPoint;
            return this;
        }

        Builder setSelectedAccountId(@Nullable CoreAccountId selectedAccountId) {
            mSelectedAccountId = selectedAccountId;
            return this;
        }

        FullscreenSigninState build() {
            return new FullscreenSigninState(this);
        }
    }
}
