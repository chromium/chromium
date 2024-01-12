// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** The set of common properties associated with dropdown suggestions. */
public @interface DropdownCommonProperties {
    /** Whether the suggestion background's top corners should be rounded. */
    public static final WritableBooleanPropertyKey BG_TOP_CORNER_ROUNDED =
            new WritableBooleanPropertyKey();

    /** Whether the suggestion background's bottom corners should be rounded. */
    public static final WritableBooleanPropertyKey BG_BOTTOM_CORNER_ROUNDED =
            new WritableBooleanPropertyKey();

    /** Whether a divider should be shown at the bottom of the suggestion. */
    public static final WritableBooleanPropertyKey SHOW_DIVIDER = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BG_TOP_CORNER_ROUNDED,
                BG_BOTTOM_CORNER_ROUNDED,
                SHOW_DIVIDER,
            };
}
