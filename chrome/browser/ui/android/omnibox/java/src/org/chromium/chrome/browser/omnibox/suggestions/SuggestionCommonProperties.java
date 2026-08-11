// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

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

    /**
     * The positional mode of a suggestion within its visual group. Used to determine which corners
     * of the suggestion background should be rounded.
     */
    @IntDef({
        PositionalMode.MIDDLE,
        PositionalMode.TOP,
        PositionalMode.BOTTOM,
        PositionalMode.SINGLE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface PositionalMode {
        int MIDDLE = 0;
        int TOP = 1;
        int BOTTOM = 2;
        int SINGLE = 3;
    }

    /** The sides of the suggestion background that are allowed to be rounded. */
    @Target(ElementType.TYPE_USE)
    @IntDef({RoundSides.NONE, RoundSides.BOTTOM_ONLY, RoundSides.TOP_AND_BOTTOM})
    @Retention(RetentionPolicy.SOURCE)
    @interface RoundSides {
        int NONE = 0;
        int BOTTOM_ONLY = 1;
        int TOP_AND_BOTTOM = 2;
    }

    /** The type of suggestion separator to draw between suggestions */
    @IntDef({GroupSeparatorType.NONE, GroupSeparatorType.GAP, GroupSeparatorType.LINE})
    @Retention(RetentionPolicy.SOURCE)
    @interface GroupSeparatorType {
        int NONE = 0;
        int GAP = 1;
        int LINE = 2;
    }

    /** Whether non-zero horizontal margins should be applied to the suggestion view. */
    WritableBooleanPropertyKey APPLY_SIDE_SPACING = new WritableBooleanPropertyKey();

    /** The positional mode of the suggestion in its group, used for corner rounding. */
    WritableIntPropertyKey BG_POSITIONAL_MODE = new WritableIntPropertyKey();

    /** The sides of the suggestion background that are allowed to be rounded. */
    WritableIntPropertyKey BG_ROUND_SIDES = new WritableIntPropertyKey();

    /** Whether dark colors should be applied to text, icons. */
    WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /** The device type for calculating the tile margin in the suggestion view. */
    WritableIntPropertyKey DEVICE_FORM_FACTOR = new WritableIntPropertyKey();

    /** The fusebox layout mode (TOOLBAR vs SUGGESTIONS_POPOVER). */
    WritableIntPropertyKey FUSEBOX_LAYOUT_MODE = new WritableIntPropertyKey();

    /** The type of group separator to show before this item. */
    WritableIntPropertyKey GROUP_SEPARATOR_TYPE = new WritableIntPropertyKey();

    /** The title text of the header above this item. */
    WritableObjectPropertyKey<String> HEADER_TITLE = new WritableObjectPropertyKey<>();

    /** The 0-based index of this suggestion in the group. */
    WritableIntPropertyKey INDEX_IN_GROUP = new WritableIntPropertyKey();

    /** The layout direction to be applied to the entire suggestion view. */
    WritableIntPropertyKey LAYOUT_DIRECTION = new WritableIntPropertyKey();

    /** The provider for omnibox resources. */
    WritableObjectPropertyKey<OmniboxResourceProvider> RESOURCE_PROVIDER =
            new WritableObjectPropertyKey<>();

    /** Whether a divider should be shown at the bottom of the suggestion. */
    WritableBooleanPropertyKey SHOW_DIVIDER = new WritableBooleanPropertyKey();

    /** The total number of visible suggestions in the group. */
    WritableIntPropertyKey TOTAL_IN_GROUP = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                APPLY_SIDE_SPACING,
                BG_POSITIONAL_MODE,
                BG_ROUND_SIDES,
                COLOR_SCHEME,
                DEVICE_FORM_FACTOR,
                FUSEBOX_LAYOUT_MODE,
                GROUP_SEPARATOR_TYPE,
                HEADER_TITLE,
                INDEX_IN_GROUP,
                LAYOUT_DIRECTION,
                RESOURCE_PROVIDER,
                SHOW_DIVIDER,
                TOTAL_IN_GROUP
            };
}
