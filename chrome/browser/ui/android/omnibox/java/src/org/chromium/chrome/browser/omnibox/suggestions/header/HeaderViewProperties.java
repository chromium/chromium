// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with the header suggestions. */
public class HeaderViewProperties {
    /** The text content to be displayed as a header text. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    /**
     * The flag to state whether to use the updated padding on suggestion header for omnibox revamp
     * phase 2.
     */
    public static final WritableBooleanPropertyKey USE_MODERNIZED_HEADER_PADDING =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TITLE, USE_MODERNIZED_HEADER_PADDING};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
