// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * An interface providing access to general information of the extension UI.
 *
 * <p>This interface is always compiled regardless of whether the underlying extension system in C++
 * is compiled or not. You can load the implementation by calling {@link #maybeCreate()} if it is
 * available.
 */
@NullMarked
public interface ExtensionService extends Destroyable {
    /** Instantiates the implementation if it is available. */
    @Nullable
    public static ExtensionService maybeCreate(ObservableSupplier<Profile> profileSupplier) {
        ExtensionService service = ServiceLoaderUtil.maybeCreate(ExtensionService.class);
        if (service == null) {
            return null;
        }
        service.initialize(profileSupplier);
        return service;
    }

    /** Initializes the service. */
    public void initialize(ObservableSupplier<Profile> profileSupplier);

    /** Whether extensions are enabled. */
    public boolean areExtensionsEnabled();
}
