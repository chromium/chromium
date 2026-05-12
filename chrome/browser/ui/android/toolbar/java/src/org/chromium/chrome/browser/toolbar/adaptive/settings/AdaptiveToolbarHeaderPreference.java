// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;

/** Fragment that allows the user to configure toolbar shortcut preferences. */
@NullMarked
public class AdaptiveToolbarHeaderPreference extends Preference implements ContainmentItem {
    public AdaptiveToolbarHeaderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.adaptive_toolbar_header_preference);
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return BackgroundStyle.CARD;
    }

    @Override
    public int getCustomBackgroundColor() {
        return SemanticColorUtils.getColorSurfaceContainerHighest(getContext());
    }
}
