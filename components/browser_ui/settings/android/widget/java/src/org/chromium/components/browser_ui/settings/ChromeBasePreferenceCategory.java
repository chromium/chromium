// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.PreferenceCategory;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A {@link PreferenceCategory} that collapses unused icon space by default. */
@NullMarked
public class ChromeBasePreferenceCategory extends PreferenceCategory {
    /** Constructor for use in Java. */
    public ChromeBasePreferenceCategory(Context context) {
        this(context, null);
    }

    /** Constructor for inflating from XML. */
    public ChromeBasePreferenceCategory(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        SettingsUtils.initializePreferenceDefaults(context, attrs, this);
    }
}
