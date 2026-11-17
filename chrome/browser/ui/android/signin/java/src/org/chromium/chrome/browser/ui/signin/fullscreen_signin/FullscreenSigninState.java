// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;

/** Immutable state of the fullscreen sign-in screen. */
@NullMarked
final class FullscreenSigninState {
    private final boolean mNativeLoaded;
    private final @Nullable AccountInfo mSelectedAccount;

    /** Creates the state before any input has arrived. */
    FullscreenSigninState() {
        mNativeLoaded = false;
        mSelectedAccount = null;
    }

    private FullscreenSigninState(Builder builder) {
        mNativeLoaded = builder.mNativeLoaded;
        mSelectedAccount = builder.mSelectedAccount;
    }

    /** Whether native initialization has completed. */
    boolean isNativeLoaded() {
        return mNativeLoaded;
    }

    /** The selected account, or null if the accounts haven't loaded yet or there are none. */
    @Nullable AccountInfo getSelectedAccount() {
        return mSelectedAccount;
    }

    Builder buildUpon() {
        return new Builder(this);
    }

    static final class Builder {
        private boolean mNativeLoaded;
        private @Nullable AccountInfo mSelectedAccount;

        private Builder(FullscreenSigninState state) {
            mNativeLoaded = state.mNativeLoaded;
            mSelectedAccount = state.mSelectedAccount;
        }

        Builder setNativeLoaded() {
            mNativeLoaded = true;
            return this;
        }

        Builder setSelectedAccount(@Nullable AccountInfo selectedAccount) {
            mSelectedAccount = selectedAccount;
            return this;
        }

        FullscreenSigninState build() {
            return new FullscreenSigninState(this);
        }
    }
}
