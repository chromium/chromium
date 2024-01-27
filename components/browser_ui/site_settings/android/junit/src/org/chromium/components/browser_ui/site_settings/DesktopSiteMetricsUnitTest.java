// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.content_settings.ContentSettingValues;

/** Unit tests for {@link DesktopSiteMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DesktopSiteMetricsUnitTest {
    private Website mSite;
    @Mock private WebsiteAddress mOrigin;
    @Mock private WebsiteAddress mEmbedder;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mSite = new Website(mOrigin, mEmbedder);
    }

    @Test
    public void testRecordDesktopSiteSettingsManuallyAdded() {
        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES,
                ContentSettingValues.BLOCK,
                "www.google.com");
        assertEquals(
                "Only REQUEST_DESKTOP_SITE type should be recorded.",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.DomainSettingAdded", 0));
        assertEquals(
                "Only REQUEST_DESKTOP_SITE type should be recorded.",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.SubDomainSettingAdded", 0));

        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingValues.BLOCK,
                "[*.]google.com");
        assertEquals(
                "DomainSettingAdded should be recorded with value false.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.DomainSettingAdded", 0));

        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingValues.ALLOW,
                "[*.]google.com");
        assertEquals(
                "DomainSettingAdded should be recorded with value true.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.DomainSettingAdded", 1));

        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingValues.BLOCK,
                "www.google.com");
        assertEquals(
                "SubDomainSettingAdded should be recorded with value false.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.SubDomainSettingAdded", 0));

        DesktopSiteMetrics.recordDesktopSiteSettingsManuallyAdded(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                ContentSettingValues.ALLOW,
                "www.google.com");
        assertEquals(
                "SubDomainSettingAdded should be recorded with value true.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop.SubDomainSettingAdded", 1));
    }

    @Test
    public void testRecordDesktopSiteSettingsChanged() {
        // SubDomain Setting
        when(mOrigin.getIsAnySubdomainPattern()).thenReturn(false);
        DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                SiteSettingsCategory.Type.THIRD_PARTY_COOKIES, ContentSettingValues.ALLOW, mSite);
        assertEquals(
                "Only REQUEST_DESKTOP_SITE type should be recorded.",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.DomainSettingChanged", 1));
        assertEquals(
                "Only REQUEST_DESKTOP_SITE type should be recorded.",
                0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.SubDomainSettingChanged", 1));

        DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, ContentSettingValues.BLOCK, mSite);
        assertEquals(
                "SubDomainSettingChanged should be recorded with value false.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.SubDomainSettingChanged", 0));

        DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, ContentSettingValues.ALLOW, mSite);
        assertEquals(
                "SubDomainSettingChanged should be recorded with value true.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.SubDomainSettingChanged", 1));

        // Domain Setting
        when(mOrigin.getIsAnySubdomainPattern()).thenReturn(true);
        DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, ContentSettingValues.BLOCK, mSite);
        assertEquals(
                "DomainSettingChanged should be recorded with value false.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.DomainSettingChanged", 0));

        DesktopSiteMetrics.recordDesktopSiteSettingsChanged(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE, ContentSettingValues.ALLOW, mSite);
        assertEquals(
                "DomainSettingChanged should be recorded with value true.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.DomainSettingChanged", 1));
    }
}
