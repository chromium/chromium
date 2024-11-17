// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/** A placeholder {@link PreferenceFragmentCompat} for use in tests. */
public class PlaceholderSettingsForTest extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    private static final ObservableSupplier<String> sPageTitle =
            new ObservableSupplierImpl<>("Placeholder Settings");

    @Override
    public void onCreatePreferences(Bundle bundle, String rootKey) {
        Context context = getPreferenceManager().getContext();
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(context);
        setPreferenceScreen(screen);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return sPageTitle;
    }
}
