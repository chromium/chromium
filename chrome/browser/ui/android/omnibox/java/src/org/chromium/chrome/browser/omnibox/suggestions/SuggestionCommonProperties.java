// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/**
 * The set of common properties associated with any omnibox suggestion.
 */
public class SuggestionCommonProperties {
    /** Whether dark colors should be applied to text, icons. */
    public static final WritableIntPropertyKey OMNIBOX_THEME = new WritableIntPropertyKey();
    /** The layout direction to be applied to the entire suggestion view. */
    public static final WritableIntPropertyKey LAYOUT_DIRECTION = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {OMNIBOX_THEME, LAYOUT_DIRECTION};
}
