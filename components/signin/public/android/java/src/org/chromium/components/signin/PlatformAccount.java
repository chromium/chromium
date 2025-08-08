// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin;

import androidx.annotation.MainThread;
import androidx.annotation.WorkerThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;
import org.chromium.google_apis.gaia.GaiaId;

/** Provides details about an account. */
@NullMarked
public interface PlatformAccount {
    /** Returns gaiaId of the PlatformAccount. */
    @MainThread
    GaiaId getId();

    /** Returns email of the PlatformAccount. */
    @MainThread
    String getEmail();

    /**
     * Returns a {@link CapabilityResponse} that indicates whether the account has the requested
     * capability or has an exception.
     */
    @WorkerThread
    @CapabilityResponse
    int fetchCapability(String capability);
}
