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
    @IntDef({Phase.LOADING, Phase.READY})
    @Retention(RetentionPolicy.SOURCE)
    @interface Phase {
        int LOADING = 0;
        int READY = 1;
    }

    private final @Phase int mPhase;
    private final boolean mNativeLoaded;
    private final boolean mPoliciesLoaded;
    private final boolean mChildStatusLoaded;
    private final boolean mAccountsLoaded;
    private final @Nullable CoreAccountId mSelectedAccountId;

    /** Creates the state before any input has arrived. */
    FullscreenSigninState() {
        mPhase = Phase.LOADING;
        mNativeLoaded = false;
        mPoliciesLoaded = false;
        mChildStatusLoaded = false;
        mAccountsLoaded = false;
        mSelectedAccountId = null;
    }

    private FullscreenSigninState(Builder builder) {
        mPhase = builder.mPhase;
        mNativeLoaded = builder.mNativeLoaded;
        mPoliciesLoaded = builder.mPoliciesLoaded;
        mChildStatusLoaded = builder.mChildStatusLoaded;
        mAccountsLoaded = builder.mAccountsLoaded;
        mSelectedAccountId = builder.mSelectedAccountId;
    }

    @Phase
    int getPhase() {
        return mPhase;
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

    /** Whether all four initial-load inputs have arrived. */
    boolean isInitialLoadComplete() {
        return mNativeLoaded && mPoliciesLoaded && mChildStatusLoaded && mAccountsLoaded;
    }

    Builder buildUpon() {
        return new Builder(this);
    }

    static final class Builder {
        private @Phase int mPhase;
        private boolean mNativeLoaded;
        private boolean mPoliciesLoaded;
        private boolean mChildStatusLoaded;
        private boolean mAccountsLoaded;
        private @Nullable CoreAccountId mSelectedAccountId;

        private Builder(FullscreenSigninState state) {
            mPhase = state.mPhase;
            mNativeLoaded = state.mNativeLoaded;
            mPoliciesLoaded = state.mPoliciesLoaded;
            mChildStatusLoaded = state.mChildStatusLoaded;
            mAccountsLoaded = state.mAccountsLoaded;
            mSelectedAccountId = state.mSelectedAccountId;
        }

        Builder setPhase(@Phase int phase) {
            mPhase = phase;
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

        Builder setSelectedAccountId(@Nullable CoreAccountId selectedAccountId) {
            mSelectedAccountId = selectedAccountId;
            return this;
        }

        FullscreenSigninState build() {
            return new FullscreenSigninState(this);
        }
    }
}
