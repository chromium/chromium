// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

public class TrackingProtectionStatusPreference extends Preference {
    private TextView mCookieStatus;
    private TextView mIpStatus;
    private TextView mFingerprintStatus;

    private boolean mStatus;

    /** Creates a new object and sets the widget layout. */
    public TrackingProtectionStatusPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mStatus = true;
        setLayoutResource(R.layout.tracking_protection_status);
    }

    /** Gets triggered when the view elements are created. */
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mCookieStatus = (TextView) holder.findViewById(R.id.cookie_status);
        mIpStatus = (TextView) holder.findViewById(R.id.ip_status);
        mFingerprintStatus = (TextView) holder.findViewById(R.id.fingerprint_status);
        setTrackingProtectionStatus(mStatus);
    }

    public void setTrackingProtectionStatus(boolean enabled) {
        mStatus = enabled;
        if (mCookieStatus == null) return;

        Drawable cookieIcon =
                AppCompatResources.getDrawable(
                        getContext(), enabled ? R.drawable.tp_cookie_off : R.drawable.tp_cookie);
        Drawable ipIcon =
                AppCompatResources.getDrawable(
                        getContext(), enabled ? R.drawable.tp_ip_off : R.drawable.tp_ip);
        Drawable fingerprintIcon =
                AppCompatResources.getDrawable(
                        getContext(),
                        enabled ? R.drawable.tp_fingerprint_off : R.drawable.tp_fingerprint);

        // TODO(b/330745124): Show a distinction between 3PC being blocked and limited.
        mCookieStatus.setText(
                enabled
                        ? R.string.page_info_tracking_protection_site_info_button_label_limited
                        : R.string.page_info_tracking_protection_site_info_button_label_allowed);
        mCookieStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(cookieIcon, null, null, null);
        mIpStatus.setText(
                enabled
                        ? R.string.page_info_tracking_protection_ip_protection_on
                        : R.string.page_info_tracking_protection_ip_protection_off);
        mIpStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(ipIcon, null, null, null);
        mFingerprintStatus.setText(
                enabled
                        ? R.string.page_info_tracking_protection_anti_fingerprinting_on
                        : R.string.page_info_tracking_protection_anti_fingerprinting_off);
        mFingerprintStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(
                fingerprintIcon, null, null, null);
    }
}
