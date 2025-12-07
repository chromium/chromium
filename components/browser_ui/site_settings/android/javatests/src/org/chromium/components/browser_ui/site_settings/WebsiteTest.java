// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;

/** Unit tests for {@link Website}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class WebsiteTest {
    private static final String CHROME_EXTENSION_ORIGIN =
            "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";
    private static final String VALID_HOST = "example.com";
    private static final String HTTPS_ORIGIN = "https://" + VALID_HOST;

    @Test
    @SmallTest
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
    @SmallTest
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
}
