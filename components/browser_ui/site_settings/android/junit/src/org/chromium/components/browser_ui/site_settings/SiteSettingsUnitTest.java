// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

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
import org.chromium.components.permissions.PermissionsAndroidFeatureList;

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
}
