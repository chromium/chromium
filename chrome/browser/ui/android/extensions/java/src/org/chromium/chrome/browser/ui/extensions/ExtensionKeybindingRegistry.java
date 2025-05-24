// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.view.KeyEvent;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Manages the registration and handling of keybindings (keyboard shortcuts) defined by chrome
 * extensions.
 */
@NullMarked
public interface ExtensionKeybindingRegistry extends Destroyable {
    /** Initializes the registry. */
    void initialize(Profile profile);

    /**
     * Handles the KeyEvent corresponding to an extension shortcut. This triggers a matching command
     * execution, if any.
     *
     * @param event The KeyEvent to handle.
     */
    boolean handleKeyEvent(KeyEvent event);

    /** Factory method to obtain an instance of the registry, if available. */
    @Nullable
    public static ExtensionKeybindingRegistry maybeCreate(Profile profile) {
        ExtensionKeybindingRegistry registry =
                ServiceLoaderUtil.maybeCreate(ExtensionKeybindingRegistry.class);
        if (registry == null) {
            return null;
        }
        registry.initialize(profile);
        return registry;
    }
}
