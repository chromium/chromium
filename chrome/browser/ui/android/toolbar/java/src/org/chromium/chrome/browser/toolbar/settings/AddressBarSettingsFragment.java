// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.base.DeviceFormFactor;

/** Fragment for address bar settings. */
@NullMarked
public class AddressBarSettingsFragment extends ChromeBaseSettingsFragment {
    @VisibleForTesting static final String PREF_ADDRESS_BAR_HEADER = "address_bar_header";
    @VisibleForTesting static final String PREF_ADDRESS_BAR_PREFERENCE = "address_bar_preference";
    @VisibleForTesting static final String PREF_ADDRESS_BAR_TITLE = "address_bar_title";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.address_bar_settings);
        CharSequence summary = getTitle(getContext());
        mPageTitle.set(summary.toString());
        findPreference(PREF_ADDRESS_BAR_TITLE).setTitle(summary);
        overrideDescriptionIfFoldable();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        overrideDescriptionIfFoldable();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void overrideDescriptionIfFoldable() {
        if (BuildInfo.getInstance().isFoldable) {
            findPreference(PREF_ADDRESS_BAR_TITLE)
                    .setSummary(R.string.address_bar_settings_description_foldable);
            // Ensure the preference disabled state reflects device folded state.
            findPreference(PREF_ADDRESS_BAR_PREFERENCE)
                    .setEnabled(!DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
        }
    }

    public static String getTitle(Context context) {
        return context.getString(R.string.address_bar_settings);
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
