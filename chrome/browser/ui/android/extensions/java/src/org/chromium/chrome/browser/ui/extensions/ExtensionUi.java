// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A collection of utility functions common to the extension UI.
 *
 * <p>This interface is always compiled regardless of whether the underlying extension system in C++
 * is compiled or not.
 */
@NullMarked
public class ExtensionUi {
    private static @Nullable ExtensionUiBackend sBackend = ExtensionUiBackend.maybeCreate();

    private ExtensionUi() {}

    /** Injects a backend for testing. */
    public static void setBackendForTesting(@Nullable ExtensionUiBackend backend) {
        sBackend = backend;
    }

    /**
     * Returns whether the extension UI should be enabled for the given profile.
     *
     * <p>You can assume that the return value never changes for the lifetime of the profile.
     */
    public static boolean isEnabled(Profile profile) {
        if (sBackend == null) {
            return false;
        }
        return sBackend.isEnabled(profile);
    }
}
