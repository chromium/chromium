// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties controlling the state of the list of suggestion items. */
@NullMarked
@interface SuggestionListProperties {
    /**
     * Whether the activity window is focused. See {@link Activity#onWindowFocusChanged(boolean)}.
     */
    WritableBooleanPropertyKey ACTIVITY_WINDOW_FOCUSED = new WritableBooleanPropertyKey();

    WritableBooleanPropertyKey ALLOW_PARKING_AT_SENTINEL = new WritableBooleanPropertyKey();

    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();

    WritableFloatPropertyKey CHILD_TRANSLATION_Y = new WritableFloatPropertyKey();

    /**
     * Specifies the color scheme. It can be light or dark because of a publisher defined color,
     * incognito, or the default theme that follows dynamic colors.
     */
    WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /**
     * Whether the dropdown container should always be visible, even if there's no suggestions to
     * show.
     */
    WritableBooleanPropertyKey CONTAINER_ALWAYS_VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the dropdown should draw over top of the anchor view. */
    WritableBooleanPropertyKey DRAW_OVER_ANCHOR = new WritableBooleanPropertyKey();

    /** The listener that will receive the new height of the suggestion list in pixels. */
    WritableObjectPropertyKey<Callback<Integer>> DROPDOWN_HEIGHT_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The listener that will be invoked whenever the User scrolls the list. */
    WritableObjectPropertyKey<Runnable> DROPDOWN_SCROLL_LISTENER =
            new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<Callback<Integer>> DROPDOWN_SCROLL_OFFSET_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The listener that will be invoked whenever the User scrolls the list to the top. */
    WritableObjectPropertyKey<Runnable> DROPDOWN_SCROLL_TO_TOP_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The embedder for the suggestion list. */
    ReadableObjectPropertyKey<OmniboxSuggestionsDropdownEmbedder> EMBEDDER =
            new ReadableObjectPropertyKey<>();

    /** The layout mode of the fusebox; see {@link FuseboxLayoutMode} */
    WritableIntPropertyKey FUSEBOX_LAYOUT_MODE = new WritableIntPropertyKey();

    /**
     * The observer that will receive notifications that the user is interacting with an item on the
     * Suggestions list.
     */
    WritableObjectPropertyKey<OmniboxSuggestionsDropdown.GestureObserver> GESTURE_OBSERVER =
            new WritableObjectPropertyKey<>();

    /** Whether the suggestions are being rendered on a large screen. */
    WritableBooleanPropertyKey IS_LARGE_SCREEN = new WritableBooleanPropertyKey();

    /** Whether the list encompasses the final set of suggestions for the current user query. */
    WritableBooleanPropertyKey LIST_IS_FINAL = new WritableBooleanPropertyKey();

    /**
     * The listener that will receive notifications when the user navigates the Suggestions list
     * using tab or arrow keys.
     */
    WritableObjectPropertyKey<OmniboxSuggestionsDropdown.NavigationListener> NAVIGATION_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the Omnibox session is active and Suggestions may be shown. */
    WritableBooleanPropertyKey OMNIBOX_SESSION_ACTIVE = new WritableBooleanPropertyKey();

    WritableObjectPropertyKey<Void> RESET_SELECTION =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    /** The resource provider for omnibox suggestions. */
    WritableObjectPropertyKey<OmniboxResourceProvider> RESOURCE_PROVIDER =
            new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey ROUND_TOP_CORNERS = new WritableBooleanPropertyKey();

    /** The list of models controlling the state of the suggestion items. */
    ReadableObjectPropertyKey<ModelList> SUGGESTION_MODELS = new ReadableObjectPropertyKey<>();

    /** On-screen placement of the Toolbar. */
    WritableIntPropertyKey TOOLBAR_POSITION = new WritableIntPropertyKey();

    /** Whether to apply a left margin offset to the suggestions container. */
    WritableBooleanPropertyKey APPLY_MARGIN_FOR_LEFT_SIDE_BAR = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                // keep-sorted start
                ACTIVITY_WINDOW_FOCUSED,
                ALLOW_PARKING_AT_SENTINEL,
                ALPHA,
                APPLY_MARGIN_FOR_LEFT_SIDE_BAR,
                CHILD_TRANSLATION_Y,
                COLOR_SCHEME,
                CONTAINER_ALWAYS_VISIBLE,
                DRAW_OVER_ANCHOR,
                DROPDOWN_HEIGHT_CHANGE_LISTENER,
                DROPDOWN_SCROLL_LISTENER,
                DROPDOWN_SCROLL_OFFSET_LISTENER,
                DROPDOWN_SCROLL_TO_TOP_LISTENER,
                EMBEDDER,
                FUSEBOX_LAYOUT_MODE,
                GESTURE_OBSERVER,
                IS_LARGE_SCREEN,
                LIST_IS_FINAL,
                NAVIGATION_LISTENER,
                OMNIBOX_SESSION_ACTIVE,
                RESET_SELECTION,
                RESOURCE_PROVIDER,
                ROUND_TOP_CORNERS,
                SUGGESTION_MODELS,
                TOOLBAR_POSITION,
                // keep-sorted end
            };
}
