// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Unit tests for {@link ToolbarLongPressMenuHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
public final class ToolbarLongPressMenuHandlerUnitTest {
    @Mock private Context mContext;
    @Mock private Resources mResources;

    private ToolbarLongPressMenuHandler mToolbarLongPressMenuHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(1).when(mResources).getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        doReturn(mResources).when(mContext).getResources();

        mToolbarLongPressMenuHandler = new ToolbarLongPressMenuHandler(mContext);
    }

    @Test
    @SmallTest
    public void testNoListenerOnTablet() {
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        mToolbarLongPressMenuHandler = new ToolbarLongPressMenuHandler(mContext);

        assertNull(mToolbarLongPressMenuHandler.getOnLongClickListener());
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testReturnListenerOnPhone() {
        assertNotNull(mToolbarLongPressMenuHandler.getOnLongClickListener());
    }
}
