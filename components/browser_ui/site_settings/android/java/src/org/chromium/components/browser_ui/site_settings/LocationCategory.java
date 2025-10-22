// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.content_public.browser.BrowserContextHandle;

/** A class for dealing with the Geolocation category. */
@NullMarked
public class LocationCategory extends SiteSettingsCategory {
    @IntDef({GrantType.UNSPECIFIED, GrantType.APPROXIMATE, GrantType.PRECISE})
    private @interface GrantType {
        // Used for location grants without the precise/approximate option.
        int UNSPECIFIED = 0;

        // Used for location grants with the precise/approximate option.
        int APPROXIMATE = 1;
        int PRECISE = 2;
    }

    private final @GrantType int mGrantType;

    /**
     * Construct a LocationCategory.
     *
     * <p>This constructor does not specify whether precise or approximate location was granted, so
     * the returned LocationCategory will not enable specific warnings on the mismatch of the
     * granted granularity at the OS vs site level.
     */
    public LocationCategory(BrowserContextHandle browserContextHandle) {
        super(
                browserContextHandle,
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                android.Manifest.permission.ACCESS_COARSE_LOCATION);
        mGrantType = GrantType.UNSPECIFIED;
    }

    /**
     * Construct a LocationCategory.
     *
     * <p>This constructor should be used when the kApproximateGeolocationPermission flag is enabled
     * and the user granted explicitly either precise or approximate location, in order to show
     * detailed warnings depending on the mismatch of the grants between site and OS.
     */
    public LocationCategory(BrowserContextHandle browserContextHandle, boolean forPreciseLocation) {
        super(
                browserContextHandle,
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                android.Manifest.permission.ACCESS_COARSE_LOCATION);
        mGrantType = forPreciseLocation ? GrantType.PRECISE : GrantType.APPROXIMATE;
    }

    @Override
    public boolean permissionOnInAndroid(String permission, Context context) {
        return LocationUtils.getInstance().hasAndroidLocationPermission();
    }

    /**
     * @return Whether precise location OS permission is blocked.
     */
    public boolean isPreciseLocationBlockedInOs() {
        return !LocationUtils.getInstance().hasAndroidFineLocationPermission();
    }

    /**
     * @return Whether this category should show a warning about a mismatch between the location
     * accuracy granted for the site and the accuracy granted at the OS level to Chrome.
     */
    public boolean hasPreciseOnlyBlockedWarning(Context context) {
        return mGrantType == GrantType.PRECISE
                && enabledGlobally()
                && enabledForChrome(context)
                && isPreciseLocationBlockedInOs();
    }

    @Override
    protected boolean enabledGlobally() {
        return LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @Override
    public boolean showPermissionBlockedMessage(Context context) {
        if (enabledForChrome(context) && enabledGlobally()) {
            return false;
        }

        // The only time we don't want to show location as blocked in system is when Chrome also
        // blocks Location by policy (because then turning it on in the system isn't going to
        // turn on location in Chrome).
        return WebsitePreferenceBridge.isContentSettingEnabled(
                        getBrowserContextHandle(), ContentSettingsType.GEOLOCATION)
                || WebsitePreferenceBridge.isContentSettingUserModifiable(
                        getBrowserContextHandle(), ContentSettingsType.GEOLOCATION);
    }

    @Override
    protected @Nullable Intent getIntentToEnableOsGlobalPermission(Context context) {
        if (enabledGlobally()) return null;
        return LocationUtils.getInstance().getSystemLocationSettingsIntent();
    }

    @Override
    protected String getMessageForEnablingOsGlobalPermission(Context context) {
        Resources resources = context.getResources();
        if (enabledForChrome(context)) {
            return resources.getString(R.string.android_location_off_globally);
        }
        return resources.getString(R.string.android_location_also_off_globally);
    }
}
