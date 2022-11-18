// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import androidx.recyclerview.widget.RecyclerView.RecycledViewPool;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The base set of properties for the Carousel suggestions. */
public class BaseCarouselSuggestionViewProperties {
    /** Action Icons description. */
    public static final WritableObjectPropertyKey<List<ListItem>> TILES =
            new WritableObjectPropertyKey<>();

    /** The header title to be applied to the suggestion. */
    public static final WritableObjectPropertyKey<CharSequence> TITLE =
            new WritableObjectPropertyKey<>();

    /** Controls whether the Header should be shown. */
    public static final WritableBooleanPropertyKey SHOW_TITLE = new WritableBooleanPropertyKey();

    /** Controls whether the carousel should have horizontal fade effect. */
    public static final WritableBooleanPropertyKey HORIZONTAL_FADE =
            new WritableBooleanPropertyKey();

    /** The recycler view pool to be appled to the carousel recycler view. */
    public static final WritableObjectPropertyKey<RecycledViewPool> RECYCLED_VIEW_POOL =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS =
            new PropertyKey[] {TITLE, SHOW_TITLE, TILES, HORIZONTAL_FADE, RECYCLED_VIEW_POOL};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
