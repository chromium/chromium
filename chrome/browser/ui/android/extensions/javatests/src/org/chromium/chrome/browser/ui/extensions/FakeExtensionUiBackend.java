// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A fake implementation of {@link ExtensionUiBackend}.
 *
 * <p>{@link ExtensionUiBackendImpl}, the default implementation of {@link ExtensionUiBackend},
 * calls into {@link ExtensionActionsBridge}, so you may want to consider {@link
 * FakeExtensionActionsBridge} instead if your tests run on extension-enabled builds only. This fake
 * is useful on testing a class that uses {@link ExtensionUi} and is compiled on all platforms
 * because {@link ExtensionActionsBridge} is not compiled if extensions are disabled.
 */
@NullMarked
public class FakeExtensionUiBackend implements ExtensionUiBackend {
    private boolean mEnabled;

    public FakeExtensionUiBackend() {
        mEnabled = ExtensionsBuildflags.ENABLE_DESKTOP_ANDROID_EXTENSIONS;
    }

    /**
     * Sets whether the extension UI is enabled or not. It applies to all profiles.
     *
     * <p>By default, it returns true if extensions are enabled on this build; otherwise false.
     */
    public void setEnabled(boolean enabled) {
        mEnabled = enabled;
    }

    @Override
    public boolean isEnabled(Profile profile) {
        return mEnabled;
    }
}
