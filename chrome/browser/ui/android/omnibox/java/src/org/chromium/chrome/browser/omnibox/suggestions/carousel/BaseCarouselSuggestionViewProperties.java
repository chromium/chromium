// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Collection;

/** The base set of properties for the Carousel suggestions. */
public class BaseCarouselSuggestionViewProperties {
    /** Action Icons description. */
    public static final WritableObjectPropertyKey<Collection<ListItem>> TILES =
            new WritableObjectPropertyKey<>();

    /** The header title to be applied to the suggestion. */
    public static final WritableObjectPropertyKey<CharSequence> TITLE =
            new WritableObjectPropertyKey<>();

    /** Controls whether the Header should be shown. */
    public static final WritableBooleanPropertyKey SHOW_TITLE = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TITLE, SHOW_TITLE, TILES};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
