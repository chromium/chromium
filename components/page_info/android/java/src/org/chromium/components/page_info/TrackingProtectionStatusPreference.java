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

import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;

import java.util.ArrayList;
import java.util.List;

public class TrackingProtectionStatusPreference extends Preference {

    private static class ShowAction {
        public @TrackingProtectionFeatureType int type;
        public @CookieControlsEnforcement int enforcement;

        public ShowAction(
                @TrackingProtectionFeatureType int type,
                @CookieControlsEnforcement int enforcement) {
            this.type = type;
            this.enforcement = enforcement;
        }
    }

    private TextView mCookieStatus;
    private TextView mIpStatus;
    private TextView mFingerprintStatus;

    // Which managed icon to show for each element, if any.
    private Drawable mManagedCookieIcon;
    private Drawable mManagedIpIcon;
    private Drawable mManagedFingerprintIcon;

    private boolean mBlockAll3PC;
    private boolean mStatus;
    private List<ShowAction> mElementsToShow;

    /** Constructor for Java code. */
    public TrackingProtectionStatusPreference(Context context) {
        this(context, null);
    }

    /** Constructor from xml. */
    public TrackingProtectionStatusPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mElementsToShow = new ArrayList<ShowAction>();
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
        for (ShowAction action : mElementsToShow) {
            setVisible(action.type, true, action.enforcement);
        }
        mElementsToShow.clear();
        setTrackingProtectionStatus(mStatus);
    }

    public void setBlockAll3PC(boolean block) {
        mBlockAll3PC = block;
    }

    public void setVisible(@TrackingProtectionFeatureType int type, boolean visible) {
        setVisible(type, visible, CookieControlsEnforcement.NO_ENFORCEMENT);
    }

    public void setVisible(
            @TrackingProtectionFeatureType int type,
            boolean visible,
            @CookieControlsEnforcement int enforcement) {
        // View is not created completely. Delay this until it is.
        if (mCookieStatus == null) {
            // Initially everything is hidden, so only need to remember what to show.
            if (!visible) return;
            var action = new ShowAction(type, enforcement);
            mElementsToShow.add(action);
            return;
        }
        Drawable managedIcon = null;
        switch (enforcement) {
            case CookieControlsEnforcement.NO_ENFORCEMENT:
            case CookieControlsEnforcement.ENFORCED_BY_EXTENSION:
            case CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT:
                managedIcon = null;
                break;
            case CookieControlsEnforcement.ENFORCED_BY_POLICY:
                managedIcon =
                        AppCompatResources.getDrawable(getContext(), R.drawable.enterprise_icon);
                break;
            case CookieControlsEnforcement.ENFORCED_BY_COOKIE_SETTING:
                managedIcon =
                        AppCompatResources.getDrawable(
                                getContext(), R.drawable.ic_settings_gear_24dp);
                break;
            default:
                assert false : "Invalid CookieControlsEnforcement value";
        }
        int visibility = visible ? View.VISIBLE : View.GONE;
        switch (type) {
            case TrackingProtectionFeatureType.THIRD_PARTY_COOKIES:
                mCookieStatus.setVisibility(visibility);
                mManagedCookieIcon = managedIcon;
                return;
            case TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION:
                mFingerprintStatus.setVisibility(visibility);
                mManagedFingerprintIcon = managedIcon;
                return;
            case TrackingProtectionFeatureType.IP_PROTECTION:
                mIpStatus.setVisibility(visibility);
                mManagedIpIcon = managedIcon;
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
        mCookieStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(
                cookieIcon, null, mManagedCookieIcon, null);
        mIpStatus.setText(
                enabled
                        ? R.string.page_info_tracking_protection_ip_protection_on
                        : R.string.page_info_tracking_protection_ip_protection_off);
        mIpStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(
                ipIcon, null, mManagedIpIcon, null);
        mFingerprintStatus.setText(
                enabled
                        ? R.string.page_info_tracking_protection_anti_fingerprinting_on
                        : R.string.page_info_tracking_protection_anti_fingerprinting_off);
        mFingerprintStatus.setCompoundDrawablesRelativeWithIntrinsicBounds(
                fingerprintIcon, null, mManagedFingerprintIcon, null);
    }
}
