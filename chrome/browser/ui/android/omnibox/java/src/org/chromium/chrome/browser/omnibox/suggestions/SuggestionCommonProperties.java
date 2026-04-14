// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The set of common properties associated with any omnibox suggestion. */
@NullMarked
public @interface SuggestionCommonProperties {
    /** Enum for identifying the device type */
    @IntDef({FormFactor.UNKNOWN, FormFactor.PHONE, FormFactor.TABLET})
    @Retention(RetentionPolicy.SOURCE)
    @interface FormFactor {
        int UNKNOWN = 0;
        int PHONE = 1;
        int TABLET = 2;
    }

    /** Whether dark colors should be applied to text, icons. */
    WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /** The layout direction to be applied to the entire suggestion view. */
    WritableIntPropertyKey LAYOUT_DIRECTION = new WritableIntPropertyKey();

    /** The device type for calculating the tile margin in the suggestion view. */
    WritableIntPropertyKey DEVICE_FORM_FACTOR = new WritableIntPropertyKey();

    /** Whether the suggestion background's top corners should be rounded. */
    WritableBooleanPropertyKey BG_TOP_CORNER_ROUNDED = new WritableBooleanPropertyKey();

    /** Whether the suggestion background's bottom corners should be rounded. */
    WritableBooleanPropertyKey BG_BOTTOM_CORNER_ROUNDED = new WritableBooleanPropertyKey();

    /** Whether a divider should be shown at the bottom of the suggestion. */
    WritableBooleanPropertyKey SHOW_DIVIDER = new WritableBooleanPropertyKey();

    /** Whether to show a gap from the previous suggestion group. */
    WritableBooleanPropertyKey SHOW_GROUP_SEPARATOR = new WritableBooleanPropertyKey();

    /** The title text of the header above this item. */
    WritableObjectPropertyKey<String> HEADER_TITLE = new WritableObjectPropertyKey<>();

    /** The 0-based index of this suggestion in the group. */
    WritableIntPropertyKey INDEX_IN_GROUP = new WritableIntPropertyKey();

    /** The total number of visible suggestions in the group. */
    WritableIntPropertyKey TOTAL_IN_GROUP = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                COLOR_SCHEME,
                LAYOUT_DIRECTION,
                DEVICE_FORM_FACTOR,
                BG_TOP_CORNER_ROUNDED,
                BG_BOTTOM_CORNER_ROUNDED,
                SHOW_DIVIDER,
                SHOW_GROUP_SEPARATOR,
                HEADER_TITLE,
                INDEX_IN_GROUP,
                TOTAL_IN_GROUP
            };
}
