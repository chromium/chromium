// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.view.View.OnClickListener;

import androidx.annotation.DrawableRes;

import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.components.omnibox.action.OmniboxPedal;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering the pedal suggestion view.
 */
public class PedalSuggestionViewProperties {
    /**
     * Describes the pedal Icon.
     */
    public static final class PedalIcon {
        public final @DrawableRes int iconRes;
        public final boolean tintWithTextColor;

        /**
         * Create a new action for suggestion.
         *
         * @param iconRes The resource id pointing to the icon.
         * @param tintWithTextColor tintWithTextColor If true then the image view will be tinted
         *         with the primary text color.
         */
        public PedalIcon(@DrawableRes int iconRes, boolean tintWithTextColor) {
            this.iconRes = iconRes;
            this.tintWithTextColor = tintWithTextColor;
        }
    }

    /** Omnibox Pedal description. */
    public static final WritableObjectPropertyKey<OmniboxPedal> PEDAL =
            new WritableObjectPropertyKey();

    /** Omnibox Pedal's drawable resource id. */
    public static final WritableObjectPropertyKey<PedalIcon> PEDAL_ICON =
            new WritableObjectPropertyKey<>();

    /** Callback invoked when user clicks the pedal. */
    public static final WritableObjectPropertyKey<OnClickListener> ON_PEDAL_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {PEDAL, PEDAL_ICON, ON_PEDAL_CLICK};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionViewProperties.ALL_KEYS);
}