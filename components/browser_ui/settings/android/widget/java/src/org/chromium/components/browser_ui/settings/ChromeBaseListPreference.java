// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.ListPreference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A {@link ListPreference} that collapses unused icon space by default. */
@NullMarked
public class ChromeBaseListPreference extends ListPreference {
    /** Constructor for use in Java. */
    public ChromeBaseListPreference(Context context) {
        this(context, null);
    }

    /** Constructor for inflating from XML. */
    public ChromeBaseListPreference(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        SettingsUtils.initializePreferenceDefaults(context, attrs, this);
    }
}
