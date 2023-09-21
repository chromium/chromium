// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting.test_support;

import android.content.ContentResolver;
import android.provider.Settings;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** This shadow class lets tests manipulate the global settings for Android. */
@Implements(Settings.Global.class)
public class ShadowGlobalSettings {
    // Whether handwriting is enabled. 1 = enabled, 0 = disabled, -1 = unset.
    private static int sHandwritingEnabled = -1;

    @Implementation
    public static int getInt(ContentResolver contentResolver, String name, int def) {
        if ("stylus_handwriting_enabled".equals(name)) {
            return sHandwritingEnabled;
        }
        return def;
    }

    public static void setHandwritingEnabled(boolean value) {
        sHandwritingEnabled = value ? 1 : 0;
    }
}
