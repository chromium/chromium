// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.provider.Settings;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SigninUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninUtilsTest {
    @Test
    @SmallTest
    public void testOpenSettingsForAllAccounts() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();

        assertTrue(SigninUtils.openSettingsForAllAccounts(activity));

        Intent intent = shadowOf(activity).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(Settings.ACTION_SYNC_SETTINGS, intent.getAction());
        assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK,
                intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);
    }
}
