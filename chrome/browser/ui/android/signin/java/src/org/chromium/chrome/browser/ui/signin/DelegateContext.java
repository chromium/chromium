// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import android.os.Bundle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * {@link DelegateContext} Allows for signin-flow delegate-specific state to be persisted across
 * activity recreation. This ensures that {@link
 * BottomSheetSigninAndHistorySyncCoordinator.Delegate} callbacks can be correctly resumed if the
 * activity is killed while the user is in the "add account" screen.
 */
@NullMarked
public abstract class DelegateContext {
    /** Returns a {@link Bundle} containing the state to be persisted. */
    public abstract Bundle toBundle();

    @Override
    public abstract boolean equals(@Nullable Object object);

    @Override
    public abstract int hashCode();
}
