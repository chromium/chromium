// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AndroidTaskUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
public class AndroidTaskUtilsUnitTest {
    private static final String REPOSITION_PERMISSION =
            "android.permission.REPOSITION_SELF_WINDOWS";

    @Test
    public void testCanRepositionSelfWindows_permissionGranted_returnsTrue() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(REPOSITION_PERMISSION);
        assertTrue(AndroidTaskUtils.canRepositionSelfWindows());
    }

    @Test
    public void testCanRepositionSelfWindows_permissionDenied_returnsFalse() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .denyPermissions(REPOSITION_PERMISSION);
        assertFalse(AndroidTaskUtils.canRepositionSelfWindows());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testCanRepositionSelfWindows_belowCinnamonBun_returnsFalse() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(REPOSITION_PERMISSION);
        assertFalse(AndroidTaskUtils.canRepositionSelfWindows());
    }
}
