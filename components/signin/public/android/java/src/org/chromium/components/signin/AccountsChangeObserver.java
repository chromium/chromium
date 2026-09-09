// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;

/**
 * Observer that receives account change notifications. Use {@link AccountManagerFacade#addObserver}
 * and {@link AccountManagerFacade#removeObserver} to update registrations.
 *
 * @deprecated New code should implement either {@link IdentityManager.Observer} or {@link
 *     ProfileDataCache.Observer} instead.
 */
@Deprecated
@NullMarked
public interface AccountsChangeObserver {
    /** Called on every change to the accounts or if getting accounts fails. */
    @MainThread
    void onAccountsChanged();
}
