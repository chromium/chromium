// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class which manages the supplier and UnownedUserData for a {@link
 * BottomSheetSigninAndHistorySyncCoordinator} specifically for web sign-in.
 */
@NullMarked
public class WebSigninAndHistorySyncCoordinatorSupplier {
    private static final UnownedUserDataKey<
                    MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
            KEY = new UnownedUserDataKey<>();

    private static @Nullable MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>
            sInstanceForTesting;

    /**
     * Retrieves an {@link MonotonicObservableSupplier} from the given host.
     *
     * <p>TODO(crbug.com/489068414): Attach to the UnownedUserDataHost associated with this tab, not
     * this window. {@link TabInterface} in tab_interface.h is not yet exposed to Java. This would
     * help each tab create and destroy its own sign-in coordinator.
     */
    public static @Nullable BottomSheetSigninAndHistorySyncCoordinator getValueOrNullFrom(
            WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting.get();
        }
        MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier =
                KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier) {
        KEY.attachToHost(host, supplier);
    }

    /** Detach from all hosts. */
    public static void destroy(
            MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    /** Sets the instance for testing. */
    public static void setInstanceForTesting(
            @Nullable MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>
                    supplier) {
        sInstanceForTesting = supplier;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private WebSigninAndHistorySyncCoordinatorSupplier() {}
}
