// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import androidx.annotation.IntDef;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class which manages the supplier and UnownedUserData for a {@link
 * BottomSheetSigninAndHistorySyncCoordinator} specifically for bottom sheet sign-in flows.
 */
// TODO(crbug.com/516671304): Add support for checking whether there is an ongoing sign-in and
// migrate to only having a single coordinator instance.
@NullMarked
public class BottomSheetSigninAndHistorySyncCoordinatorSupplier {
    @IntDef({SupplierFlow.WEB_SIGNIN, SupplierFlow.EXTENSIONS})
    public @interface SupplierFlow {
        int WEB_SIGNIN = 0;
        int EXTENSIONS = 1;
        int NUM_ENTRIES = 2;
    }

    private static final UnownedUserDataKey<
                    MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
            WEB_SIGNIN_KEY = new UnownedUserDataKey<>();
    private static final UnownedUserDataKey<
                    MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
            EXTENSIONS_KEY = new UnownedUserDataKey<>();

    private static @Nullable MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>
            sInstanceForTesting;

    /**
     * Retrieves an {@link MonotonicObservableSupplier} from the given host.
     *
     * <p>TODO(crbug.com/489068414): Attach to the UnownedUserDataHost associated with this tab, not
     * this window. {@link TabInterface} in tab_interface.h is not yet exposed to Java. This would
     * help each tab create and destroy its own sign-in coordinator.
     */
    public static @Nullable BottomSheetSigninAndHistorySyncCoordinator getValueForFlow(
            WindowAndroid windowAndroid, @SupplierFlow int flow) {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting.get();
        }
        UnownedUserDataKey<MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
                key = getKeyForFlow(flow);
        MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier =
                key.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attachForFlow(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier,
            @SupplierFlow int flow) {
        UnownedUserDataKey<MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
                key = getKeyForFlow(flow);
        key.attachToHost(host, supplier);
    }

    /** Detach from all hosts. */
    public static void destroyForFlow(
            MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator> supplier,
            @SupplierFlow int flow) {
        getKeyForFlow(flow).detachFromAllHosts(supplier);
    }

    /** Sets the instance for testing. */
    public static void setInstanceForTesting(
            @Nullable MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>
                    supplier) {
        sInstanceForTesting = supplier;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private static UnownedUserDataKey<
                    MonotonicObservableSupplier<BottomSheetSigninAndHistorySyncCoordinator>>
            getKeyForFlow(@SupplierFlow int flow) {
        switch (flow) {
            case SupplierFlow.WEB_SIGNIN:
                return WEB_SIGNIN_KEY;
            case SupplierFlow.EXTENSIONS:
                return EXTENSIONS_KEY;
            default:
                throw new IllegalArgumentException("Invalid SupplierFlow: " + flow);
        }
    }

    private BottomSheetSigninAndHistorySyncCoordinatorSupplier() {}
}
