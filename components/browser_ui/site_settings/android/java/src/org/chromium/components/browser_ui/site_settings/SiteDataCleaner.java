// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Encapsulates clearing the data of {@link Website}s and {@link WebsiteGroup}s.
 * Requires native library to be initialized.
 */
public class SiteDataCleaner {
    /**
     * Clears the data of the specified site.
     *
     * @param finishCallback is called when finished.
     */
    public static void clearData(
            SiteSettingsDelegate siteSettingsDelegate, Website site, Runnable finishCallback) {
        String origin = site.getAddress().getOrigin();
        var browserContextHandle = siteSettingsDelegate.getBrowserContextHandle();
        WebsitePreferenceBridgeJni.get().clearCookieData(browserContextHandle, origin);

        if (!siteSettingsDelegate.isBrowsingDataModelFeatureEnabled()) {
            WebsitePreferenceBridgeJni.get().clearMediaLicenses(browserContextHandle, origin);
        }

        WebsitePreferenceBridgeJni.get().clearBannerData(browserContextHandle, origin);
        site.clearAllStoredData(siteSettingsDelegate, finishCallback::run);
    }

    /**
     * Clears the data for each of the sites in a given group.
     *
     * @param finishCallback is called when the entire operation is finished.
     */
    public static void clearData(
            SiteSettingsDelegate siteSettingsDelegate,
            WebsiteGroup group,
            Runnable finishCallback) {

        final AtomicInteger callbacksReceived = new AtomicInteger(0);
        List<Website> sites = group.getWebsites();
        final int websitesCount = sites.size();
        final Runnable singleWebsiteCallback =
                () -> {
                    if (callbacksReceived.incrementAndGet() >= websitesCount) {
                        finishCallback.run();
                    }
                };
        for (Website site : sites) {
            clearData(siteSettingsDelegate, site, singleWebsiteCallback);
        }
    }

    /** Resets the permissions of the specified site. */
    public static void resetPermissions(BrowserContextHandle browserContextHandle, Website site) {
        // Clear the permissions.
        for (ContentSettingException exception : site.getContentSettingExceptions()) {
            site.setContentSetting(
                    browserContextHandle,
                    exception.getContentSettingType(),
                    ContentSettingValues.DEFAULT);
        }
        for (PermissionInfo info : site.getPermissionInfos()) {
            site.setContentSetting(
                    browserContextHandle,
                    info.getContentSettingsType(),
                    ContentSettingValues.DEFAULT);
        }

        for (ChosenObjectInfo info : site.getChosenObjectInfo()) {
            info.revoke(browserContextHandle);
        }

        for (var exceptions : site.getEmbeddedPermissions().values()) {
            for (var exception : exceptions) {
                exception.setContentSetting(browserContextHandle, ContentSettingValues.DEFAULT);
            }
        }
    }

    /** Resets the permissions for each of the sites in a given group. */
    public static void resetPermissions(
            BrowserContextHandle browserContextHandle, WebsiteGroup group) {
        for (Website site : group.getWebsites()) {
            resetPermissions(browserContextHandle, site);
        }
    }
}
