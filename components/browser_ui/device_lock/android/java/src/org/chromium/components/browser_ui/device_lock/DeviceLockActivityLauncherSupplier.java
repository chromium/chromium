// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class provides a way to access {@link DeviceLockActivityLauncher} from {@link
 * WindowAndroid}.
 */
@NullMarked
public class DeviceLockActivityLauncherSupplier {
    private static final UnownedUserDataKey<ObservableSupplier<DeviceLockActivityLauncher>> KEY =
            new UnownedUserDataKey<>();

    /**
     * Return {@link DeviceLockActivityLauncher} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable ObservableSupplier<DeviceLockActivityLauncher> from(
            WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Return the {@link DeviceLockActivityLauncher} associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable DeviceLockActivityLauncher get(WindowAndroid windowAndroid) {
        ObservableSupplier<DeviceLockActivityLauncher> supplier = from(windowAndroid);
        return supplier != null ? supplier.get() : null;
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, ObservableSupplier<DeviceLockActivityLauncher> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(ObservableSupplier<DeviceLockActivityLauncher> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private DeviceLockActivityLauncherSupplier() {}
}
