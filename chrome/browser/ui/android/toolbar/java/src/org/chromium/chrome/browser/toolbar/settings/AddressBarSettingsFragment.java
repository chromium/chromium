// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment for address bar settings. */
public class AddressBarSettingsFragment extends ChromeBaseSettingsFragment {
    @VisibleForTesting
    public static final String PREF_ADDRESS_BAR_PREFERENCE = "address_bar_preference";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.address_bar_settings);
        mPageTitle.set(getString(R.string.address_bar_settings));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }
}
