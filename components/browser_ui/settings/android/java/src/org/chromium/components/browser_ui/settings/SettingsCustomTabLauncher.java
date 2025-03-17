// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;

/** CCT-related helpers and interfaces. */
@NullMarked
public interface SettingsCustomTabLauncher {
    /**
     * Interface for injecting a SettingsCustomTabHelper to a fragment. This is useful for fragments
     * that need to launch a Custom tab.
     */
    interface SettingsCustomTabLauncherClient {
        /** Sets an instance of {@link SettingsCustomTabLauncher} in a fragment. */
        void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher);
    }

    /**
     * Opens a given Url in a CCT.
     *
     * @param context A {@link Context} object.
     * @param url Url to open in the CCT.
     */
    void openUrlInCct(Context context, String url);
}
