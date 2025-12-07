// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.RadioGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * A radio button group Preference used for {@link GeolocationSetting} permission. It contains 2
 * options: a {@link RadioButtonWithDescription} that represent Precise location, and a {@link
 * RadioButtonWithDescription} that represents Approximate location.
 */
@NullMarked
public class LocationPermissionOptionsPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener {
    private @MonotonicNonNull BrowserContextHandle mBrowserContextHandle;
    private @MonotonicNonNull Website mSite;
    private @MonotonicNonNull GeolocationSetting mSetting;
    private @MonotonicNonNull RadioButtonWithDescription mPrecise;
    private @MonotonicNonNull RadioButtonWithDescription mApproximate;
    private @MonotonicNonNull LocationPermissionSubpageSettings mSubpage;
    private boolean mIsPreciseSelected;
    private @NonNull CharSequence mPreciseSummary = "";

    public LocationPermissionOptionsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        setLayoutResource(R.layout.location_permission_options_preference);

        // Make unselectable, otherwise LocationPermissionOptionsPreference is treated as
        // one selectable Preference, instead of two selectable radio buttons.
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        RadioGroup radioGroup = (RadioGroup) holder.findViewById(R.id.location_access_group);
        assumeNonNull(radioGroup);
        radioGroup.setOnCheckedChangeListener(this);

        mPrecise = (RadioButtonWithDescription) holder.findViewById(R.id.precise);
        assumeNonNull(mPrecise);
        mPrecise.setDescriptionText(mPreciseSummary);
        mApproximate = (RadioButtonWithDescription) holder.findViewById(R.id.approximate);
        assumeNonNull(mApproximate);

        RadioButtonWithDescription selectedButton = mIsPreciseSelected ? mPrecise : mApproximate;
        selectedButton.setChecked(true);
    }

    public void initialize(
            @NonNull BrowserContextHandle browserContextHandle,
            @NonNull Website site,
            @NonNull LocationPermissionSubpageSettings subpage) {
        mBrowserContextHandle = browserContextHandle;
        mSite = site;
        mSubpage = subpage;

        PermissionInfo info = mSite.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
        assumeNonNull(info);

        mSetting = info.getGeolocationSetting(browserContextHandle);

        // Subpage should not be visible if location permission is blocked.
        assert mSetting.mApproximate == ContentSetting.ALLOW;

        mIsPreciseSelected = mSetting.mPrecise == ContentSetting.ALLOW;
        notifyChanged();
    }

    @Override
    public void onCheckedChanged(@NonNull RadioGroup radioGroup, int i) {
        assumeNonNull(mPrecise);
        assumeNonNull(mSetting);
        assumeNonNull(mSite);
        assumeNonNull(mBrowserContextHandle);
        assumeNonNull(mSubpage);

        mIsPreciseSelected = mPrecise.isChecked();
        boolean isPermissionAllowed = mSetting.mApproximate == ContentSetting.ALLOW;
        int approximate = isPermissionAllowed ? ContentSetting.ALLOW : ContentSetting.BLOCK;
        int precise = mIsPreciseSelected ? approximate : ContentSetting.BLOCK;
        assumeNonNull(mSite.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS))
                .setGeolocationSetting(
                        mBrowserContextHandle, new GeolocationSetting(approximate, precise));
        mSubpage.setUpOsWarningPreferences();
    }

    public @Nullable RadioButtonWithDescription getApproximateButtonForTesting() {
        return mApproximate;
    }

    public @Nullable RadioButtonWithDescription getPreciseButtonForTesting() {
        return mPrecise;
    }

    public void setPreciseSummary(@NonNull CharSequence summary) {
        mPreciseSummary = summary;
        notifyChanged();
    }
}
