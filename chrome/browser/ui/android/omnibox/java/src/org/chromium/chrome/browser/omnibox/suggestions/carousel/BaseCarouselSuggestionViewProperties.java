// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
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

    /** Specifies carousel padding dimensions. */
    static final ReadableIntPropertyKey TOP_PADDING = new ReadableIntPropertyKey();

    static final ReadableIntPropertyKey BOTTOM_PADDING = new ReadableIntPropertyKey();

    /** Specifies whether carousel's background should match this used by all suggestions. */
    static final ReadableBooleanPropertyKey APPLY_BACKGROUND = new ReadableBooleanPropertyKey();

    static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {
                TOP_PADDING,
                BOTTOM_PADDING,
                APPLY_BACKGROUND,
                TILES,
                HORIZONTAL_FADE,
                ITEM_WIDTH,
                CONTENT_DESCRIPTION
            };

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
