// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

/**
 * A placeholder {@link PreferenceFragmentCompat} for use in tests.
 */
public class PlaceholderSettingsForTest extends PreferenceFragmentCompat {
    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        getActivity().setTitle("Placeholder Settings");

        Context context = getPreferenceManager().getContext();
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(context);
        setPreferenceScreen(screen);
    }
}
