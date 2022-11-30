// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

/**
 * Interface for injecting SettingsLauncher to a fragment. It is useful for modularized fragments
 * that need access to SettingsLauncher. See: go/clank-modularize-settings-launcher.
 */
public interface FragmentSettingsLauncher {
    /**
     * Sets an instance of SettingsLauncher in a fragment.
     *
     * @param settingsLauncher The SettingsLauncher that is injected.
     */
    void setSettingsLauncher(SettingsLauncher settingsLauncher);
}
