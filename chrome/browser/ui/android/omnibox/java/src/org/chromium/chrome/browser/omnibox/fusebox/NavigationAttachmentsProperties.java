// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** The properties associated with the Navigation Attachments bar. */
@NullMarked
class NavigationAttachmentsProperties {
    /** The adapter for the attachments RecyclerView. */
    public static final WritableObjectPropertyKey<SimpleRecyclerViewAdapter> ADAPTER =
            new WritableObjectPropertyKey<>();

    /** Whether the add button is visible. */
    public static final WritableBooleanPropertyKey ADD_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the attachments toolbar is visible. */
    public static final WritableBooleanPropertyKey ATTACHMENTS_TOOLBAR_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the attachments RecyclerView is visible. */
    public static final WritableBooleanPropertyKey ATTACHMENTS_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Tracks the {@link AutocompleteRequestType}. */
    public static final WritableObjectPropertyKey<Integer> AUTOCOMPLETE_REQUEST_TYPE =
            new WritableObjectPropertyKey<>();

    /** Whether the navigation type toggle is changeable. */
    public static final WritableBooleanPropertyKey AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Autocomplete Request Type button. */
    public static final WritableObjectPropertyKey<Runnable> AUTOCOMPLETE_REQUEST_TYPE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Add button. */
    public static final WritableObjectPropertyKey<Runnable> BUTTON_ADD_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the AI Mode button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_AI_MODE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Camera button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_CAMERA_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the Clipboard button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_CLIPBOARD_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Clipboard button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_CLIPBOARD_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the File button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_FILE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Gallery button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_GALLERY_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the recent tabs header is visible. */
    public static final WritableBooleanPropertyKey RECENT_TABS_HEADER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether to show the dedicated AIMode button directly in the Fusebox. */
    public static final WritableBooleanPropertyKey SHOW_DEDICATED_MODE_BUTTON =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ADAPTER,
        ADD_BUTTON_VISIBLE,
        ATTACHMENTS_TOOLBAR_VISIBLE,
        ATTACHMENTS_VISIBLE,
        AUTOCOMPLETE_REQUEST_TYPE,
        AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE,
        AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
        BUTTON_ADD_CLICKED,
        POPUP_AI_MODE_CLICKED,
        POPUP_CAMERA_CLICKED,
        POPUP_CLIPBOARD_BUTTON_VISIBLE,
        POPUP_CLIPBOARD_CLICKED,
        POPUP_FILE_CLICKED,
        POPUP_GALLERY_CLICKED,
        RECENT_TABS_HEADER_VISIBLE,
        SHOW_DEDICATED_MODE_BUTTON
    };
}
