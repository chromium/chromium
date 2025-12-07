// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;

/**
 * A base class for preferences that contain a group of radio buttons and need to be individually
 * contained. This class automatically handles making the preference background transparent so that
 * the styled radio button children can be seen.
 */
@NullMarked
public abstract class ContainedRadioButtonGroupPreference extends Preference
        implements ContainmentItem {

    public ContainedRadioButtonGroupPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        // This ensures the Preference itself is transparent, allowing the inner containment to
        // be visible.
        return BackgroundStyle.NONE;
    }
}
