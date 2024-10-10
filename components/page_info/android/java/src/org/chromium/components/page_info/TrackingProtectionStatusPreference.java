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

import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionBlockingStatus;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;

import java.util.ArrayList;
import java.util.List;

public class TrackingProtectionStatusPreference extends Preference {

    private static class UpdateAction {
        public TrackingProtectionFeature feature;
        public boolean visible;

        public UpdateAction(TrackingProtectionFeature feature, boolean visible) {
            this.feature = feature;
            this.visible = visible;
        }
    }

    private TextView mCookieStatus;
    private TextView mIpStatus;
    private TextView mFingerprintStatus;

    private List<UpdateAction> mStatusUpdates;

    /** Constructor for Java code. */
    public TrackingProtectionStatusPreference(Context context) {
        this(context, null);
    }

    /** Constructor from xml. */
    public TrackingProtectionStatusPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mStatusUpdates = new ArrayList<UpdateAction>();
        setLayoutResource(R.layout.tracking_protection_status);
    }

    /** Gets triggered when the view elements are created. */
    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        mCookieStatus = (TextView) holder.findViewById(R.id.cookie_status);
        mIpStatus = (TextView) holder.findViewById(R.id.ip_status);
        mFingerprintStatus = (TextView) holder.findViewById(R.id.fingerprint_status);
        for (UpdateAction action : mStatusUpdates) {
            updateStatus(action.feature, action.visible);
        }
        mStatusUpdates.clear();
    }

    private Drawable managedIconForEnforcement(@CookieControlsEnforcement int enforcement) {
        switch (enforcement) {
            case CookieControlsEnforcement.NO_ENFORCEMENT:
            case CookieControlsEnforcement.ENFORCED_BY_EXTENSION:
            case CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT:
                return null;
            case CookieControlsEnforcement.ENFORCED_BY_POLICY:
                return AppCompatResources.getDrawable(getContext(), R.drawable.enterprise_icon);
            case CookieControlsEnforcement.ENFORCED_BY_COOKIE_SETTING:
                return AppCompatResources.getDrawable(
                        getContext(), R.drawable.ic_settings_gear_24dp);
            default:
                assert false : "Invalid CookieControlsEnforcement value";
                return null;
        }
    }

    private int statusIconForFeature(TrackingProtectionFeature feature) {
        switch (feature.featureType) {
            case TrackingProtectionFeatureType.THIRD_PARTY_COOKIES:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.ALLOWED:
                        return R.drawable.tp_cookie;
                    case TrackingProtectionBlockingStatus.BLOCKED:
                    case TrackingProtectionBlockingStatus.LIMITED:
                        return R.drawable.tp_cookie_off;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for 3PC";
                        return 0;
                }
            case TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.ALLOWED:
                        return R.drawable.tp_fingerprint;
                    case TrackingProtectionBlockingStatus.LIMITED:
                        return R.drawable.tp_fingerprint_off;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for FPP";
                        return 0;
                }
            case TrackingProtectionFeatureType.IP_PROTECTION:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.VISIBLE:
                        return R.drawable.tp_ip;
                    case TrackingProtectionBlockingStatus.HIDDEN:
                        return R.drawable.tp_ip_off;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for IPP";
                        return 0;
                }
            default:
                assert false : "Invalid TrackingProtectionFeatureType";
                return 0;
        }
    }

    private int statusStringForFeature(TrackingProtectionFeature feature) {
        switch (feature.featureType) {
            case TrackingProtectionFeatureType.THIRD_PARTY_COOKIES:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.ALLOWED:
                        return R.string
                                .page_info_tracking_protection_site_info_button_label_allowed;
                    case TrackingProtectionBlockingStatus.BLOCKED:
                        return R.string
                                .page_info_tracking_protection_site_info_button_label_blocked;
                    case TrackingProtectionBlockingStatus.LIMITED:
                        return R.string
                                .page_info_tracking_protection_site_info_button_label_limited;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for 3PC";
                        return 0;
                }
            case TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.ALLOWED:
                        return R.string.page_info_tracking_protection_anti_fingerprinting_off;
                    case TrackingProtectionBlockingStatus.LIMITED:
                        return R.string.page_info_tracking_protection_anti_fingerprinting_on;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for FPP";
                        return 0;
                }
            case TrackingProtectionFeatureType.IP_PROTECTION:
                switch (feature.status) {
                    case TrackingProtectionBlockingStatus.VISIBLE:
                        return R.string.page_info_tracking_protection_ip_protection_off;
                    case TrackingProtectionBlockingStatus.HIDDEN:
                        return R.string.page_info_tracking_protection_ip_protection_on;
                    default:
                        assert false : "Invalid TrackingProtectionBlockingStatus value for IPP";
                        return 0;
                }
            default:
                assert false : "Invalid TrackingProtectionFeatureType";
                return 0;
        }
    }

    public void updateStatus(TrackingProtectionFeature feature, boolean visible) {
        // View is not created completely. Delay this until it is.
        if (mCookieStatus == null) {
            var action = new UpdateAction(feature, visible);
            mStatusUpdates.add(action);
            return;
        }
        // Fetch the individual UI elements corresponding to the new state.
        Drawable statusIcon =
                AppCompatResources.getDrawable(getContext(), statusIconForFeature(feature));
        int stringRes = statusStringForFeature(feature);
        Drawable managedIcon = managedIconForEnforcement(feature.enforcement);
        int visibility = visible ? View.VISIBLE : View.GONE;

        TextView viewToUpdate = null;
        switch (feature.featureType) {
            case TrackingProtectionFeatureType.THIRD_PARTY_COOKIES:
                viewToUpdate = mCookieStatus;
                break;
            case TrackingProtectionFeatureType.FINGERPRINTING_PROTECTION:
                viewToUpdate = mFingerprintStatus;
                break;
            case TrackingProtectionFeatureType.IP_PROTECTION:
                viewToUpdate = mIpStatus;
                break;
            default:
                assert false : "Invalid TrackingProtectionFeatureType";
        }
        viewToUpdate.setVisibility(visibility);
        viewToUpdate.setText(stringRes);
        viewToUpdate.setCompoundDrawablesRelativeWithIntrinsicBounds(
                statusIcon, null, managedIcon, null);
    }
}
