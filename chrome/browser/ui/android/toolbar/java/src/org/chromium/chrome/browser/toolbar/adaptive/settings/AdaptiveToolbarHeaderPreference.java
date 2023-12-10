// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.Preference;

import org.chromium.chrome.browser.toolbar.R;

/** Fragment that allows the user to configure toolbar shorcut preferences. */
public class AdaptiveToolbarHeaderPreference extends Preference {
    public AdaptiveToolbarHeaderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.adaptive_toolbar_header_preference);
    }
}
