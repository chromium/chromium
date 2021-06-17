// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with rendering the default suggestion view. */
public class TailSuggestionViewProperties {
    /** The text content to be displayed as a tail suggestion. */
    public static final WritableObjectPropertyKey<SuggestionSpannable> TEXT =
            new WritableObjectPropertyKey<>();
    /** The text content to be used to replace contents of the Omnibox. */
    public static final WritableObjectPropertyKey<String> FILL_INTO_EDIT =
            new WritableObjectPropertyKey<>();
    /** Manager taking care of suggestions alignment. */
    public static final WritableObjectPropertyKey<AlignmentManager> ALIGNMENT_MANAGER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TEXT, FILL_INTO_EDIT, ALIGNMENT_MANAGER};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, BaseSuggestionViewProperties.ALL_KEYS);
}
