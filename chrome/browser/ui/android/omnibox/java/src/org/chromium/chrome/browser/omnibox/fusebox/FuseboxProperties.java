// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** The properties associated with the Fusebox bar. */
@NullMarked
class FuseboxProperties {
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
    public static final WritableObjectPropertyKey<@AutocompleteRequestType Integer>
            AUTOCOMPLETE_REQUEST_TYPE = new WritableObjectPropertyKey<>();

    /** Whether the navigation type toggle is changeable. */
    public static final WritableBooleanPropertyKey AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Autocomplete Request Type button. */
    public static final WritableObjectPropertyKey<Runnable> AUTOCOMPLETE_REQUEST_TYPE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Add button. */
    public static final WritableObjectPropertyKey<Runnable> BUTTON_ADD_CLICKED =
            new WritableObjectPropertyKey<>();

    /** The variant of {@link BrandedColorScheme} to apply to the UI elements. */
    public static final WritableObjectPropertyKey<@BrandedColorScheme Integer> COLOR_SCHEME =
            new WritableObjectPropertyKey<>();

    /** Whether the UI is in compact mode. */
    public static final WritableBooleanPropertyKey COMPACT_UI = new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the "add current tab" button */
    public static final WritableObjectPropertyKey<Runnable> CURRENT_TAB_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /**
     * Whether the current tab button is enabled or disabled. Being disabled still leaves it
     * visible, but with a greyed out color and not interactable.
     */
    public static final WritableBooleanPropertyKey CURRENT_TAB_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /**
     * The favicon of the underlying tab which this button would add. Can be null, in which case a
     * fallback will be used instead.
     */
    public static final WritableObjectPropertyKey<@Nullable Bitmap> CURRENT_TAB_BUTTON_FAVICON =
            new WritableObjectPropertyKey<>();

    /** Whether the current tab button is visible. */
    public static final WritableBooleanPropertyKey CURRENT_TAB_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

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

    /** Whether the create image button is enabled or disabled. */
    public static final WritableBooleanPropertyKey POPUP_CREATE_IMAGE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the create image button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_CREATE_IMAGE_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the 'Create Image' button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_CREATE_IMAGE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the File button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_FILE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the File button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_FILE_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the File button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_FILE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Gallery button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_GALLERY_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the tab picker button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_TAB_PICKER_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the tab picker button in the popup. */
    public static final WritableBooleanPropertyKey POPUP_TAB_PICKER_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the recent tabs header is visible. */
    public static final WritableBooleanPropertyKey RECENT_TABS_HEADER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether to show the dedicated AIMode button directly in the Fusebox. */
    public static final WritableBooleanPropertyKey SHOW_DEDICATED_MODE_BUTTON =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        // go/keep-sorted start
        ADAPTER,
        ADD_BUTTON_VISIBLE,
        ATTACHMENTS_TOOLBAR_VISIBLE,
        ATTACHMENTS_VISIBLE,
        AUTOCOMPLETE_REQUEST_TYPE,
        AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE,
        AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
        BUTTON_ADD_CLICKED,
        COLOR_SCHEME,
        COMPACT_UI,
        CURRENT_TAB_BUTTON_CLICKED,
        CURRENT_TAB_BUTTON_ENABLED,
        CURRENT_TAB_BUTTON_FAVICON,
        CURRENT_TAB_BUTTON_VISIBLE,
        POPUP_AI_MODE_CLICKED,
        POPUP_CAMERA_CLICKED,
        POPUP_CLIPBOARD_BUTTON_VISIBLE,
        POPUP_CLIPBOARD_CLICKED,
        POPUP_CREATE_IMAGE_BUTTON_ENABLED,
        POPUP_CREATE_IMAGE_BUTTON_VISIBLE,
        POPUP_CREATE_IMAGE_CLICKED,
        POPUP_FILE_BUTTON_ENABLED,
        POPUP_FILE_BUTTON_VISIBLE,
        POPUP_FILE_CLICKED,
        POPUP_GALLERY_CLICKED,
        POPUP_TAB_PICKER_CLICKED,
        POPUP_TAB_PICKER_ENABLED,
        RECENT_TABS_HEADER_VISIBLE,
        SHOW_DEDICATED_MODE_BUTTON
        // go/keep-sorted end
    };
}
