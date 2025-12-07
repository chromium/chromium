// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.content_public.browser.BrowserContextHandle;

/** A class for dealing with the Geolocation category. */
@NullMarked
public class LocationCategory extends SiteSettingsCategory {
    private final boolean mForPreciseGrant;

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
        mForPreciseGrant = forPreciseLocation;
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
        return canShowPreciseOnlyBlockedWarning()
                && enabledGlobally()
                && enabledForChrome(context)
                && isPreciseLocationBlockedInOs();
    }

    @Override
    protected String getMessageForEnablingOsPerAppPermission(Context context, String appName) {
        if (hasPreciseOnlyBlockedWarning(context)) {
            return context.getString(R.string.android_turn_on_geo_precise_permission);
        }
        return super.getMessageForEnablingOsPerAppPermission(context, appName);
    }

    @Override
    protected boolean enabledGlobally() {
        return LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    @Override
    public boolean showPermissionBlockedMessage(Context context) {
        if (enabledForChrome(context) && enabledGlobally()) {
            return canShowPreciseOnlyBlockedWarning() && isPreciseLocationBlockedInOs();
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
    protected boolean shouldShowPerAppWarning(Context context) {
        return (!enabledForChrome(context)
                || (canShowPreciseOnlyBlockedWarning() && isPreciseLocationBlockedInOs()));
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

    private boolean canShowPreciseOnlyBlockedWarning() {
        return PermissionsAndroidFeatureMap.isEnabled(
                        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
                && mForPreciseGrant;
    }
}
