// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with the header suggestions. */
@interface HeaderViewProperties {
    /** The text content to be displayed as a header text. */
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {TITLE};

    static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
