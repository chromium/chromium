// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.VisibleForTesting;

import java.util.concurrent.atomic.AtomicReference;

/**
 * This class groups all the {@link AccountInfoService} instance manipulation
 * methods in one place.
 */
public final class AccountInfoServiceProvider {
    private static final AtomicReference<AccountInfoService> sInstance = new AtomicReference<>();
    private static final AtomicReference<AccountInfoService> sTestingInstance =
            new AtomicReference<>();

    /**
     * Initializes the singleton {@link AccountInfoService} instance.
     */
    public static void init(
            IdentityManager identityManager, AccountTrackerService accountTrackerService) {
        sInstance.set(new AccountInfoServiceImpl(identityManager, accountTrackerService));
    }

    /**
     * Gets the singleton {@link AccountInfoService} instance.
     */
    public static AccountInfoService get() {
        if (sTestingInstance.get() != null) {
            return sTestingInstance.get();
        }
        if (sInstance.get() == null) {
            throw new RuntimeException("The AccountInfoService is not yet initialized!");
        }
        return sInstance.get();
    }

    @VisibleForTesting
    public static void setInstanceForTests(AccountInfoService accountInfoService) {
        sTestingInstance.set(accountInfoService);
    }

    @VisibleForTesting
    public static void resetForTests() {
        sTestingInstance.set(null);
        sInstance.set(null);
    }

    private AccountInfoServiceProvider() {}
}
