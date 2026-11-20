// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory.Type;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.device.DeviceFeatureList;

/** Unit tests for {@link SettingsActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SiteSettingsUnitTest {

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private SettingsIndexData mSearchIndexData;

    @Mock private Context mContext;
    @Mock private SiteSettingsDelegate mDelegate;

    @Before
    public void setUp() {
        FeatureOverrides.Builder overrides = FeatureOverrides.newBuilder();
        overrides.enable(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);
        overrides.enable(DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES);
        overrides.apply();

        for (@Type int prefCategory = 0; prefCategory < Type.NUM_ENTRIES; prefCategory++) {
            if (SiteSettingsCategory.contentSettingsType(prefCategory) < 0) continue;
            doReturn(true).when(mDelegate).isCategoryVisible(eq(prefCategory));
        }
    }

    @Test
    public void testSearchIndexProvider_removeInvisibleCategory() {
        doReturn(false).when(mDelegate).isCategoryVisible(eq(Type.CAMERA));
        SiteSettings.updateDynamicPreferences(mContext, mDelegate, mSearchIndexData);
        String key = SiteSettingsCategory.preferenceKey(Type.CAMERA);
        var indexProvider = SiteSettings.SEARCH_INDEX_DATA_PROVIDER;
        verify(mSearchIndexData).removeEntry(indexProvider.getUniqueId(key));
    }

    @Test
    public void testSensorsTriStateSettingDescriptionIds() {
        FeatureOverrides.newBuilder()
                .enable(DeviceFeatureList.SENSORS_ALLOW_ASK_BLOCK_PERMISSION_MODEL)
                .enable(DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)
                .apply();
        int[] extraSensorsIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(
                        ContentSettingsType.SENSORS);
        assertNotNull(extraSensorsIds);
        assertEquals(3, extraSensorsIds.length);
        assertEquals(R.string.website_settings_motion_and_light_sensors_allow, extraSensorsIds[0]);
        assertEquals(R.string.website_settings_motion_and_light_sensors_ask, extraSensorsIds[1]);
        assertEquals(R.string.website_settings_motion_and_light_sensors_block, extraSensorsIds[2]);

        FeatureOverrides.newBuilder()
                .enable(DeviceFeatureList.SENSORS_ALLOW_ASK_BLOCK_PERMISSION_MODEL)
                .disable(DeviceFeatureList.GENERIC_SENSOR_EXTRA_CLASSES)
                .apply();
        int[] motionOnlyIds =
                ContentSettingsResources.getTriStateSettingDescriptionIDs(
                        ContentSettingsType.SENSORS);
        assertNotNull(motionOnlyIds);
        assertEquals(3, motionOnlyIds.length);
        assertEquals(R.string.website_settings_motion_sensors_allow, motionOnlyIds[0]);
        assertEquals(R.string.website_settings_motion_sensors_ask, motionOnlyIds[1]);
        assertEquals(R.string.website_settings_motion_sensors_block, motionOnlyIds[2]);
    }
}
