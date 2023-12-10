// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsiteAddress.ANY_SUBDOMAIN_PATTERN;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.content_settings.ContentSettingValues;

/** Metrics recording functions for {@link SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE}. */
public final class DesktopSiteMetrics {
    /**
     * Records when a user manually adds a domain or subdomain level Request Desktop Setting from
     * the Add site dialog in Site settings.
     * @param type The {@link SiteSettingsCategory.Type} of the current Site settings category.
     * @param setting The {@link ContentSettingValues} of the newly added setting.
     * @param hostname The hostname of the newly added setting.
     */
    public static void recordDesktopSiteSettingsManuallyAdded(
            @SiteSettingsCategory.Type int type,
            @ContentSettingValues int setting,
            String hostname) {
        if (type != SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            return;
        }

        boolean isDesktopSite = setting == ContentSettingValues.ALLOW;
        if (hostname.startsWith(ANY_SUBDOMAIN_PATTERN)) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.RequestDesktopSite.UserSwitchToDesktop.DomainSettingAdded",
                    isDesktopSite);
        } else {
            RecordHistogram.recordBooleanHistogram(
                    "Android.RequestDesktopSite.UserSwitchToDesktop.SubDomainSettingAdded",
                    isDesktopSite);
        }
    }

    /**
     * Records when a user manually edits a domain or subdomain level Request Desktop Setting from
     * the site settings exception list.
     * @param type The {@link SiteSettingsCategory.Type} of the current Site settings category.
     * @param setting The {@link ContentSettingValues} of the newly added setting.
     * @param site The {@link Website} of the newly added setting.
     */
    public static void recordDesktopSiteSettingsChanged(
            @SiteSettingsCategory.Type int type, @ContentSettingValues int setting, Website site) {
        if (type != SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE) {
            return;
        }

        boolean isDesktopSite = setting == ContentSettingValues.ALLOW;
        if (site.getAddress().getIsAnySubdomainPattern()) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.RequestDesktopSite.DomainSettingChanged", isDesktopSite);
        } else {
            RecordHistogram.recordBooleanHistogram(
                    "Android.RequestDesktopSite.SubDomainSettingChanged", isDesktopSite);
        }
    }
}
