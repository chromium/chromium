// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.content.Context;
import android.view.View;
import android.widget.RemoteViews;
import android.widget.ScrollView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link RemoteViewsWithNightModeInflater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RemoteViewsWithNightModeInflaterUnitTest {

    @Test
    public void testInflate_NonRemoteView_Fails() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        RemoteViews remoteViews =
                new RemoteViews(context.getPackageName(), R.layout.remote_views_test_layout);

        // Pass true for isInLocalNightMode and false for isInSystemNightMode to trigger
        // inflateWithEnforcedDarkMode.
        View view =
                RemoteViewsWithNightModeInflater.inflate(
                        remoteViews,
                        /* parent= */ null,
                        /* isInLocalNightMode= */ true,
                        /* isInSystemNightMode= */ false);

        // The returned view cannot be ScrollView since it is not annotated with
        // @RemoteView.RemoteView. So inflation through RemoteViews should fail and return null.
        Assert.assertFalse("The returned view cannot be ScrollView", view instanceof ScrollView);
    }
}
