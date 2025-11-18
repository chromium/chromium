// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Fragment for address bar settings. */
@NullMarked
public class AddressBarSettingsFragment extends ChromeBaseSettingsFragment {
    @IntDef({HighlightedOption.NONE, HighlightedOption.BOTTOM_TOOLBAR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HighlightedOption {
        int NONE = 0;
        int BOTTOM_TOOLBAR = 1;
    }

    @VisibleForTesting static final String PREF_ADDRESS_BAR_HEADER = "address_bar_header";
    @VisibleForTesting static final String PREF_ADDRESS_BAR_PREFERENCE = "address_bar_preference";
    @VisibleForTesting static final String PREF_ADDRESS_BAR_TITLE = "address_bar_title";
    public static final String HIGHLIGHTED_OPTION = "AddressBarSettingsFragment.HighlightedOption";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.address_bar_settings);
        CharSequence summary = getTitle(getContext());
        mPageTitle.set(summary.toString());
        if (!ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            findPreference(PREF_ADDRESS_BAR_TITLE).setTitle(summary);
        }

        @HighlightedOption
        int highlightedOption =
                IntentUtils.safeGetInt(getArguments(), HIGHLIGHTED_OPTION, HighlightedOption.NONE);
        ((AddressBarPreference) findPreference(PREF_ADDRESS_BAR_PREFERENCE))
                .init(highlightedOption);

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
        if (DeviceInfo.isFoldable()) {
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

    /**
     * Creates an argument bundle to open the Address Bar settings page.
     *
     * @param highlightedOption The highlighted option for the Address Bar settings page.
     */
    public static Bundle createArguments(@HighlightedOption int highlightedOption) {
        Bundle result = new Bundle();
        result.putInt(HIGHLIGHTED_OPTION, highlightedOption);
        return result;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "address_bar";
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    AddressBarSettingsFragment.class.getName(), R.xml.address_bar_settings);
}
