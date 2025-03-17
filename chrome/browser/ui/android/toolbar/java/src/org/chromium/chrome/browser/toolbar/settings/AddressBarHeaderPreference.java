// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.settings;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.util.AttributeSet;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.toolbar.R;

/** The header shows on the top of {@link AddressBarPreference}. */
public class AddressBarHeaderPreference extends Preference
        implements OnSharedPreferenceChangeListener {
    private @NonNull SharedPreferencesManager mSharedPreferencesManager;
    private @NonNull ImageView mToolbarPositionImage;

    public AddressBarHeaderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.address_bar_header_preference);
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mToolbarPositionImage = (ImageView) holder.findViewById(R.id.toolbar_position_graphic);
        updateImageVisibility();
    }

    @Override
    public void onAttached() {
        super.onAttached();

        ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onDetached() {
        super.onDetached();

        ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(
            SharedPreferences sharedPreferences, @Nullable String key) {
        if (key.equals(ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED)) {
            updateImageVisibility();
        }
    }

    private void updateImageVisibility() {
        boolean showOnTop =
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.TOOLBAR_TOP_ANCHORED, true);

        mToolbarPositionImage.setSelected(showOnTop);
        int stringRes =
                showOnTop
                        ? R.string.address_bar_settings_currently_on_top
                        : R.string.address_bar_settings_currently_on_bottom;
        mToolbarPositionImage.setContentDescription(getContext().getString(stringRes));
    }

    @NonNull
    @VisibleForTesting
    ImageView getToolbarPositionImage() {
        return mToolbarPositionImage;
    }
}
