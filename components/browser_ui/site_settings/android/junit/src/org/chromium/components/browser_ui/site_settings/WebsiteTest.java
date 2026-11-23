// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.List;

/** Unit tests for {@link Website}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebsiteTest {
    private static final String CHROME_EXTENSION_ORIGIN =
            "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";
    private static final String VALID_HOST = "example.com";
    private static final String HTTPS_ORIGIN = "https://" + VALID_HOST;

    @Test
    public void testCreateContentSettingException_httpsOrigin() {
        WebsiteAddress address = WebsiteAddress.create(HTTPS_ORIGIN);
        Website website = new Website(address, null);
        ContentSettingException adsException =
                website.createContentSettingException(
                        ContentSettingsType.ADS, ContentSetting.BLOCK);
        Assert.assertEquals(HTTPS_ORIGIN, adsException.getPrimaryPattern());

        ContentSettingException jsException =
                website.createContentSettingException(
                        ContentSettingsType.JAVASCRIPT, ContentSetting.ALLOW);
        Assert.assertEquals(VALID_HOST, jsException.getPrimaryPattern());

        ContentSettingException soundException =
                website.createContentSettingException(
                        ContentSettingsType.SOUND, ContentSetting.DEFAULT);
        Assert.assertEquals(VALID_HOST, soundException.getPrimaryPattern());
    }

    @Test
    public void testCreateContentSettingException_extensionOrigin() {
        WebsiteAddress address = WebsiteAddress.create(CHROME_EXTENSION_ORIGIN);
        Website website = new Website(address, null);
        ContentSettingException adsException =
                website.createContentSettingException(
                        ContentSettingsType.ADS, ContentSetting.BLOCK);
        Assert.assertEquals(CHROME_EXTENSION_ORIGIN, adsException.getPrimaryPattern());

        ContentSettingException jsException =
                website.createContentSettingException(
                        ContentSettingsType.JAVASCRIPT, ContentSetting.ALLOW);
        Assert.assertEquals(CHROME_EXTENSION_ORIGIN, jsException.getPrimaryPattern());

        ContentSettingException soundException =
                website.createContentSettingException(
                        ContentSettingsType.SOUND, ContentSetting.DEFAULT);
        Assert.assertEquals(CHROME_EXTENSION_ORIGIN, soundException.getPrimaryPattern());
    }

    @Test
    public void testMergePermissionAndStorageInfoForTopLevelOrigin_mergesCookiesInfo() {
        WebsiteAddress targetAddress = WebsiteAddress.create("https://example.com");

        Website httpsEntry = new Website(WebsiteAddress.create("https://example.com"), null);
        httpsEntry.setCookiesInfo(new CookiesInfo(2));

        Website httpEntry = new Website(WebsiteAddress.create("http://example.com"), null);
        httpEntry.setCookiesInfo(new CookiesInfo(3));

        Website embeddedEntry =
                new Website(
                        WebsiteAddress.create("https://example.com"),
                        WebsiteAddress.create("https://embedder.com"));
        embeddedEntry.setCookiesInfo(new CookiesInfo(10));

        Website otherHostEntry = new Website(WebsiteAddress.create("https://other.com"), null);
        otherHostEntry.setCookiesInfo(new CookiesInfo(7));

        Website emptyMerged =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        targetAddress, List.of(otherHostEntry, embeddedEntry));
        Assert.assertEquals(0, emptyMerged.getNumberOfCookies());

        Website merged =
                SingleWebsiteSettings.mergePermissionAndStorageInfoForTopLevelOrigin(
                        targetAddress,
                        List.of(httpsEntry, httpEntry, embeddedEntry, otherHostEntry));
        Assert.assertEquals(5, merged.getNumberOfCookies());
    }
}
