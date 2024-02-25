// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** The set of common properties associated with any omnibox suggestion. */
public @interface SuggestionCommonProperties {
    /** Enum for identifying the device type */
    @IntDef({FormFactor.UNKNOWN, FormFactor.PHONE, FormFactor.TABLET})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FormFactor {
        int UNKNOWN = 0;
        int PHONE = 1;
        int TABLET = 2;
    }

    /** Whether dark colors should be applied to text, icons. */
    public static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /** The layout direction to be applied to the entire suggestion view. */
    public static final WritableIntPropertyKey LAYOUT_DIRECTION = new WritableIntPropertyKey();

    /** The device type for calculating the tile margin in the suggestion view. */
    public static final WritableIntPropertyKey DEVICE_FORM_FACTOR = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(
                    new PropertyKey[] {COLOR_SCHEME, LAYOUT_DIRECTION, DEVICE_FORM_FACTOR},
                    DropdownCommonProperties.ALL_KEYS);
}
