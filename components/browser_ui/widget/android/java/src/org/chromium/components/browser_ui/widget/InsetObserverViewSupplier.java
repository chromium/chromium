// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link InsetObserverView}.
 */
public class InsetObserverViewSupplier extends UnownedUserDataSupplier<InsetObserverView> {
    private static final UnownedUserDataKey<InsetObserverViewSupplier> KEY =
            new UnownedUserDataKey<InsetObserverViewSupplier>(InsetObserverViewSupplier.class);
    private static ObservableSupplierImpl<InsetObserverView> sInstanceForTesting;

    /**
     * Returns {@link InsetObserverView} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static ObservableSupplier<InsetObserverView> from(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Retrieves a {@link InsetObserverView} from {@link WindowAndroid}.  */
    public static @Nullable InsetObserverView getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        ObservableSupplier<InsetObserverView> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /** Sets an instance for testing. */
    @VisibleForTesting
    public static void setInstanceForTesting(InsetObserverView insetObserverView) {
        if (sInstanceForTesting == null) {
            sInstanceForTesting = new ObservableSupplierImpl<>();
        }
        sInstanceForTesting.set(insetObserverView);
    }

    /** Constructor. */
    public InsetObserverViewSupplier() {
        super(KEY);
    }
}
