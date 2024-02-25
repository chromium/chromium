// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * InsetObserver}.
 */
public class InsetObserverSupplier extends UnownedUserDataSupplier<InsetObserver> {
    private static final UnownedUserDataKey<InsetObserverSupplier> KEY =
            new UnownedUserDataKey<>(InsetObserverSupplier.class);
    private static ObservableSupplierImpl<InsetObserver> sInstanceForTesting;

    /** Returns {@link InsetObserver} supplier associated with the given {@link WindowAndroid}. */
    public static ObservableSupplier<InsetObserver> from(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Retrieves a {@link InsetObserver} from {@link WindowAndroid}. */
    public static @Nullable InsetObserver getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        ObservableSupplier<InsetObserver> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(InsetObserver insetObserver) {
        if (sInstanceForTesting == null) {
            sInstanceForTesting = new ObservableSupplierImpl<>();
        }
        sInstanceForTesting.set(insetObserver);
    }

    /** Constructor. */
    public InsetObserverSupplier() {
        super(KEY);
    }
}
