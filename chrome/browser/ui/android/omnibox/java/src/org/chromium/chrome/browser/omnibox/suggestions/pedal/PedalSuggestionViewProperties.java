// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.view.View.OnClickListener;

import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering the pedal suggestion view.
 */
class PedalSuggestionViewProperties {
    /** Omnibox Pedal description. */
    public static final WritableObjectPropertyKey<OmniboxPedal> PEDAL =
            new WritableObjectPropertyKey();

    /** Omnibox Pedal's drawable resource id. */
    public static final WritableIntPropertyKey PEDAL_ICON = new WritableIntPropertyKey();

    /** Callback invoked when user clicks the pedal. */
    public static final WritableObjectPropertyKey<OnClickListener> ON_PEDAL_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {PEDAL, PEDAL_ICON, ON_PEDAL_CLICK};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionViewProperties.ALL_KEYS);
}