// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ephemeraltab;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link EphemeralTabCoordinator}. */
@NullMarked
public class EphemeralTabCoordinatorSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<EphemeralTabCoordinator>>
            KEY = new UnownedUserDataKey<>();
    private static @Nullable SettableMonotonicObservableSupplier<EphemeralTabCoordinator>
            sInstanceForTesting;

    /**
     * Return {@link EphemeralTabCoordinator} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable MonotonicObservableSupplier<EphemeralTabCoordinator> from(
            WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<EphemeralTabCoordinator> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<EphemeralTabCoordinator> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(EphemeralTabCoordinator ephemeralTabCoordinator) {
        sInstanceForTesting = ObservableSuppliers.createMonotonic(ephemeralTabCoordinator);
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private EphemeralTabCoordinatorSupplier() {}
}
