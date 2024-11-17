// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

/**
 * Interface for injecting SettingsNavigation to a fragment. It is useful for fragments in
 * components that need access to SettingsNavigation. See: go/clank-modularize-settings-launcher.
 */
public interface FragmentSettingsNavigation {
    /**
     * Sets an instance of SettingsNavigation in a fragment.
     *
     * @param settingsNavigation The SettingsNavigation that is injected.
     */
    void setSettingsNavigation(SettingsNavigation settingsNavigation);
}
