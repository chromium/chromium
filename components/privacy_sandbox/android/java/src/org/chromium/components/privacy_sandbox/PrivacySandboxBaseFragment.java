// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/** Handles common dependencies for Privacy Sandbox settings */
public abstract class PrivacySandboxBaseFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage,
                SettingsCustomTabLauncher.SettingsCustomTabLauncherClient {
    private SettingsCustomTabLauncher mCustomTabLauncher;

    @Override
    public void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher) {
        mCustomTabLauncher = customTabLauncher;
    }

    /**
     * @return The launcher for CCT.
     */
    public SettingsCustomTabLauncher getCustomTabLauncher() {
        return mCustomTabLauncher;
    }
}
