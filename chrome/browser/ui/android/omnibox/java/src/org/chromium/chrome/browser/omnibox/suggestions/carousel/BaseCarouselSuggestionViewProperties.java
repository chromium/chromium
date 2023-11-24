// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The base set of properties for the Carousel suggestions. */
public @interface BaseCarouselSuggestionViewProperties {
    /** Action Icons description. */
    public static final WritableObjectPropertyKey<List<ListItem>> TILES =
            new WritableObjectPropertyKey<>();

    /** Controls whether the carousel should have horizontal fade effect. */
    static final WritableBooleanPropertyKey HORIZONTAL_FADE = new WritableBooleanPropertyKey();

    /** Specifies the width of a carousel element. */
    static final WritableIntPropertyKey ITEM_WIDTH = new WritableIntPropertyKey();

    /** Specifies the audible description of the carousel type. */
    public static final ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TILES, HORIZONTAL_FADE, ITEM_WIDTH, CONTENT_DESCRIPTION};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
