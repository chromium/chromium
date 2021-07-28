// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.atomic.AtomicReference;

/**
 * This class groups all the {@link AccountInfoService} instance manipulation
 * methods in one place.
 */
public final class AccountInfoServiceProvider {
    private static final AtomicReference<AccountInfoService> sInstance = new AtomicReference<>();
    private static @Nullable Promise<AccountInfoService> sInstancePromise;

    /**
     * Initializes the singleton {@link AccountInfoService} instance.
     */
    @MainThread
    public static void init(
            IdentityManager identityManager, AccountTrackerService accountTrackerService) {
        if (sInstance.get() != null) {
            return;
        }
        sInstance.set(new AccountInfoServiceImpl(identityManager, accountTrackerService));
        if (sInstancePromise == null) {
            sInstancePromise = Promise.fulfilled(sInstance.get());
        } else {
            sInstancePromise.fulfill(sInstance.get());
        }
    }

    /**
     * Gets the singleton {@link AccountInfoService} instance.
     *
     * This method must be invoked after {@link AccountInfoService} is initialized.
     */
    public static AccountInfoService get() {
        if (sInstance.get() == null) {
            throw new RuntimeException("The AccountInfoService is not yet initialized!");
        }
        return sInstance.get();
    }

    /**
     * Gets the {@link Promise} of the singleton {@link AccountInfoService} instance.
     *
     * This method can be invoked before {@link AccountInfoService} is initialized.
     */
    @MainThread
    public static Promise<AccountInfoService> getPromise() {
        ThreadUtils.assertOnUiThread();
        if (sInstancePromise == null) {
            sInstancePromise = new Promise<>();
        }
        return sInstancePromise;
    }

    @MainThread
    @VisibleForTesting
    public static void setInstanceForTests(AccountInfoService accountInfoService) {
        ThreadUtils.assertOnUiThread();
        sInstance.set(accountInfoService);
        sInstancePromise = Promise.fulfilled(accountInfoService);
    }

    @VisibleForTesting
    public static void resetForTests() {
        sInstance.set(null);
        sInstancePromise = null;
    }

    private AccountInfoServiceProvider() {}
}
