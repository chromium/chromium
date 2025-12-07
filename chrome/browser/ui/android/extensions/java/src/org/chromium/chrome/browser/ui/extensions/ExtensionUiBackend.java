// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * An interface providing general access to the extension UI backend in C++.
 *
 * <p>This interface is always compiled regardless of whether the underlying extension system in C++
 * is compiled or not. You can load the implementation by calling {@link #maybeCreate()} if it is
 * available.
 *
 * <p>In production code, use {@link ExtensionUi} instead of directly using this interface. In test
 * code, you can inject a fake backend by {@link ExtensionUi#setBackendForTesting()}.
 */
@NullMarked
public interface ExtensionUiBackend {
    /** Instantiates the implementation if it is available. */
    static @Nullable ExtensionUiBackend maybeCreate() {
        return ServiceLoaderUtil.maybeCreate(ExtensionUiBackend.class);
    }

    /**
     * Returns whether the extension UI should be enabled for the given profile.
     *
     * <p>You can assume that the return value never changes for the lifetime of the profile.
     */
    boolean isEnabled(Profile profile);
}
