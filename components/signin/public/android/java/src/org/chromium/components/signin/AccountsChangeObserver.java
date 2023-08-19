// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import androidx.annotation.MainThread;

/**
 * Observer that receives account change notifications. Use {@link AccountManagerFacade#addObserver}
 * and {@link AccountManagerFacade#removeObserver} to update registrations.
 */
public interface AccountsChangeObserver {
    /**
     * Called after updating {@link org.chromium.components.signin.base.CoreAccountInfo} on every
     * change to the accounts or to the error condition that occurred while getting accounts.
     */
    @MainThread
    void onCoreAccountInfosChanged();
}
