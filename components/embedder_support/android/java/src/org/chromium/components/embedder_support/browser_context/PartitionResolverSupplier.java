// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.browser_context;

import org.chromium.base.supplier.OneshotSupplierImpl;

/**
 * Shared access point for components to fetch the singleton embedder specific implementation of
 * {@link PartitionResolver}. Should be set sometime early in app lifecycle.
 */
public final class PartitionResolverSupplier {
    // The impl has thread checks and asserts set it only called once. Do not allow raw access to
    // the provider to ensure assert in get accessor is always run.
    private static final OneshotSupplierImpl<PartitionResolver> sOneshotSupplierImpl =
            new OneshotSupplierImpl<>();

    private PartitionResolverSupplier() {}

    /**
     * Sets the given singleton resolver. Should only be called once.
     * @param resolver Singleton resolver to use across the app.
     */
    public static void setInstance(PartitionResolver resolver) {
        sOneshotSupplierImpl.set(resolver);
    }

    /**
     * Fetches the previously set singleton resolver. Should only be called after being set.
     * @return The singleton resolver.
     */
    public static PartitionResolver getInstance() {
        assert sOneshotSupplierImpl.get() != null;
        return sOneshotSupplierImpl.get();
    }
}
