// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.content_settings.TrackingProtectionFeatureType;

import java.util.ArrayList;
import java.util.List;

public class TrackingProtectionStatusPreference extends Preference {
    private TextView mCookieStatus;
    private TextView mIpStatus;
    private TextView mFingerprintStatus;

    private boolean mBlockAll3PC;
    private boolean mStatus;
    private List<Integer> mElementsToShow;

    /** Creates a new object and sets the widget layout. */
    public TrackingProtectionStatusPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mElementsToShow = new ArrayList<Integer>();
        mBlockAll3PC = false;
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
        for (Integer type : mElementsToShow) {
            setVisible(type, true);
        }
    }

    public void setBlockAll3PC(boolean block) {
        mBlockAll3PC = block;
    }

    public void setVisible(@TrackingProtectionFeatureType int type, boolean visible) {
        // View is not created completely. Delay this until it is.
        if (mCookieStatus == null) {
            // Initially everything is hidden, so only need to remember what to show.
            if (!visible) return;
            mElementsToShow.add(Integer.valueOf(type));
            return;
        }
        int visibility = visible ? View.VISIBLE : View.GONE;
        switch (type) {
            case TrackingProtectionFeatureType.THIRD_PARTY_COOKIES:
                mCookieStatus.setVisibility(visibility);
                return;
            case TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION:
                mFingerprintStatus.setVisibility(visibility);
                return;
            case TrackingProtectionFeatureType.IP_PROTECTION:
                mIpStatus.setVisibility(visibility);
                return;
            default:
                assert false : "Invalid TrackingProtectionFeatureType";
        }
    }

    public void setTrackingProtectionStatus(boolean enabled) {
        mStatus = enabled;
        // View is not created completely. Delay this until it is.
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

        if (enabled) {
            mCookieStatus.setText(
                    mBlockAll3PC
                            ? R.string.page_info_tracking_protection_site_info_button_label_blocked
                            : R.string
                                    .page_info_tracking_protection_site_info_button_label_limited);
        } else {
            mCookieStatus.setText(
                    R.string.page_info_tracking_protection_site_info_button_label_allowed);
        }
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
