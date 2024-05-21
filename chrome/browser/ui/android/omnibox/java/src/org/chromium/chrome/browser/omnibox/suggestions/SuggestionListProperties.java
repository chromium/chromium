// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties controlling the state of the list of suggestion items. */
@interface SuggestionListProperties {
    static final WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();

    static final WritableFloatPropertyKey CHILD_TRANSLATION_Y = new WritableFloatPropertyKey();

    /** Whether the Omnibox session is active and Suggestions may be shown. */
    static final WritableBooleanPropertyKey OMNIBOX_SESSION_ACTIVE =
            new WritableBooleanPropertyKey();

    /** The embedder for the suggestion list. */
    static final WritableObjectPropertyKey<OmniboxSuggestionsDropdownEmbedder> EMBEDDER =
            new WritableObjectPropertyKey<>();

    /**
     * The list of models controlling the state of the suggestion items. This should never be bound
     * to the same view more than once.
     */
    static final WritableObjectPropertyKey<ModelList> SUGGESTION_MODELS =
            new WritableObjectPropertyKey<>(true);

    /** Whether the list encompasses the final set of suggestions for the current user query. */
    static final WritableBooleanPropertyKey LIST_IS_FINAL = new WritableBooleanPropertyKey();

    /**
     * Specifies the color scheme. It can be light or dark because of a publisher defined color,
     * incognito, or the default theme that follows dynamic colors.
     */
    static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /**
     * The observer that will receive notifications that the user is interacting with an item on the
     * Suggestions list.
     */
    static final WritableObjectPropertyKey<OmniboxSuggestionsDropdown.GestureObserver>
            GESTURE_OBSERVER = new WritableObjectPropertyKey<>();

    /** The listener that will receive the new height of the suggestion list in pixels. */
    static final WritableObjectPropertyKey<Callback<Integer>> DROPDOWN_HEIGHT_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The listener that will be invoked whenever the User scrolls the list. */
    static final WritableObjectPropertyKey<Runnable> DROPDOWN_SCROLL_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The listener that will be invoked whenever the User scrolls the list to the top. */
    static final WritableObjectPropertyKey<Runnable> DROPDOWN_SCROLL_TO_TOP_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the dropdown should draw over top of the anchor view. */
    static final WritableBooleanPropertyKey DRAW_OVER_ANCHOR = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                CHILD_TRANSLATION_Y,
                OMNIBOX_SESSION_ACTIVE,
                EMBEDDER,
                SUGGESTION_MODELS,
                COLOR_SCHEME,
                GESTURE_OBSERVER,
                DROPDOWN_HEIGHT_CHANGE_LISTENER,
                DROPDOWN_SCROLL_LISTENER,
                DROPDOWN_SCROLL_TO_TOP_LISTENER,
                LIST_IS_FINAL,
                DRAW_OVER_ANCHOR
            };
}
