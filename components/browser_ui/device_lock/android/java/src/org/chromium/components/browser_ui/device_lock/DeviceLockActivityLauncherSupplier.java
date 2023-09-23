// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class provides a way to access {@link DeviceLockActivityLauncher} from {@link
 * WindowAndroid}.
 */
public class DeviceLockActivityLauncherSupplier
        extends UnownedUserDataSupplier<DeviceLockActivityLauncher> {
    private static final UnownedUserDataKey<DeviceLockActivityLauncherSupplier> KEY =
            new UnownedUserDataKey<DeviceLockActivityLauncherSupplier>(
                    DeviceLockActivityLauncherSupplier.class);

    /**
     * Return {@link DeviceLockActivityLauncher} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static ObservableSupplier<DeviceLockActivityLauncher> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a ShareDelegateSupplier and attaches it to the {@link WindowAndroid} */
    public DeviceLockActivityLauncherSupplier() {
        super(KEY);
    }
}
