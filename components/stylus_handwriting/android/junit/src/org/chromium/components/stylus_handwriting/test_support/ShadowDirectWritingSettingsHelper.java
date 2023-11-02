// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting.test_support;

import android.content.Context;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.components.stylus_handwriting.DirectWritingSettingsHelper;

/**
 * The shadow of {@link DirectWritingSettingsHelper} that allows unit tests to mock static APIs.
 * Usage example:
 *
 *   @RunWith(BaseRobolectricTestRunner.class)
 *   @Config(manifest = Config.NONE, shadows = {ShadowDirectWritingSettingsHelper.class})
 *   public class MyTest {}
 */
@Implements(DirectWritingSettingsHelper.class)
public class ShadowDirectWritingSettingsHelper {
    private static boolean sEnabled;

    @Implementation
    public static boolean isEnabled(Context context) {
        return sEnabled;
    }

    public static void setEnabled(boolean enabled) {
        sEnabled = enabled;
    }
}
