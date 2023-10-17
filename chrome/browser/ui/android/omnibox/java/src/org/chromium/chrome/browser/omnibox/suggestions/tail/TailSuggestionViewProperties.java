// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering the tail suggestion view. */
@interface TailSuggestionViewProperties {
    /** The text content to be displayed as a tail suggestion. */
    static final WritableObjectPropertyKey<SuggestionSpannable> TEXT =
            new WritableObjectPropertyKey<>();

    /** The text content to be used to replace contents of the Omnibox. */
    static final WritableObjectPropertyKey<String> FILL_INTO_EDIT =
            new WritableObjectPropertyKey<>();

    /** Manager taking care of suggestions alignment. */
    static final WritableObjectPropertyKey<AlignmentManager> ALIGNMENT_MANAGER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TEXT, FILL_INTO_EDIT, ALIGNMENT_MANAGER};

    static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
