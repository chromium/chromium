// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SpacingRecyclerViewItemDecoration;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The base set of properties for the Carousel suggestions. */
@NullMarked
public @interface BaseCarouselSuggestionViewProperties {
    /** Specifies whether carousel's background should match this used by all suggestions. */
    ReadableBooleanPropertyKey APPLY_BACKGROUND = new ReadableBooleanPropertyKey();

    ReadableIntPropertyKey BOTTOM_PADDING = new ReadableIntPropertyKey();

    /** Specifies the audible description of the carousel type. */
    ReadableObjectPropertyKey<String> CONTENT_DESCRIPTION = new ReadableObjectPropertyKey<>();

    /** Specifies the width of a carousel element. */
    ReadableObjectPropertyKey<SpacingRecyclerViewItemDecoration> ITEM_DECORATION =
            new ReadableObjectPropertyKey<>();

    /** Action Icons description. */
    WritableObjectPropertyKey<List<ListItem>> TILES = new WritableObjectPropertyKey<>();

    /** Specifies carousel padding dimensions. */
    ReadableIntPropertyKey TOP_PADDING = new ReadableIntPropertyKey();

    PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {
                APPLY_BACKGROUND,
                BOTTOM_PADDING,
                CONTENT_DESCRIPTION,
                ITEM_DECORATION,
                TILES,
                TOP_PADDING
            };

    PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
