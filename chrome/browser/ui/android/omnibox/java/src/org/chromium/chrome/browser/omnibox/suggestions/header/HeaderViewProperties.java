// Copyright 2020 The Chromium Authors. All rights reserved.
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
    /** Interface that receives events from Header view. */
    interface Delegate {
        /** Invoked whenever header view is selected. */
        void onHeaderSelected();

        /** Invoked whenever header view is clicked. */
        void onHeaderClicked();
    }

    /** The runnable object that is executed whenever user taps the header suggestion. */
    public static final WritableObjectPropertyKey<Delegate> DELEGATE =
            new WritableObjectPropertyKey<>();
    /** The collapsed state of the header suggestion. */
    public static final WritableBooleanPropertyKey IS_COLLAPSED = new WritableBooleanPropertyKey();
    /** The text content to be displayed as a header text. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {DELEGATE, IS_COLLAPSED, TITLE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
