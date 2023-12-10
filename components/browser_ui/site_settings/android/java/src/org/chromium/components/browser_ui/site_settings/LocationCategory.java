// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.location.LocationUtils;
import org.chromium.content_public.browser.BrowserContextHandle;

/** A class for dealing with the Geolocation category. */
public class LocationCategory extends SiteSettingsCategory {
    public LocationCategory(BrowserContextHandle browserContextHandle) {
        super(
                browserContextHandle,
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                android.Manifest.permission.ACCESS_COARSE_LOCATION);
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
    protected Intent getIntentToEnableOsGlobalPermission(Context context) {
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
