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

    /** Action to perform when the user clicks the Camera button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_CAMERA_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the Camera button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CAMERA_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the Camera button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CAMERA_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Clipboard button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_CLIPBOARD_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the Clipboard button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CLIPBOARD_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the Clipboard button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CLIPBOARD_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the "add current tab" button */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_CURRENT_TAB_CLICKED =
            new WritableObjectPropertyKey<>();

    /**
     * Whether the current tab button is enabled or disabled. Being disabled still leaves it
     * visible, but with a greyed out color and not interactable.
     */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CURRENT_TAB_ENABLED =
            new WritableBooleanPropertyKey();

    /**
     * The favicon of the underlying tab which this button would add. Can be null, in which case a
     * fallback will be used instead.
     */
    public static final WritableObjectPropertyKey<@Nullable Bitmap>
            POPUP_ATTACH_CURRENT_TAB_FAVICON = new WritableObjectPropertyKey<>();

    /** Whether the current tab button is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CURRENT_TAB_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the File button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_FILE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the File button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_FILE_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the File button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_FILE_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Gallery button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_GALLERY_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the Gallery button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_GALLERY_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the Gallery button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_GALLERY_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the tab picker button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_TAB_PICKER_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the tab picker button in the popup. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_TAB_PICKER_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the tab picker button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_TAB_PICKER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the AI Mode button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_TOOL_AI_MODE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the AI Mode button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_AI_MODE_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the AI Mode button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_AI_MODE_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the 'Create Image' button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_TOOL_CREATE_IMAGE_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the create image button is enabled or disabled. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_CREATE_IMAGE_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the create image button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_CREATE_IMAGE_VISIBLE =
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
        AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
        BUTTON_ADD_CLICKED,
        COLOR_SCHEME,
        COMPACT_UI,
        POPUP_ATTACH_CAMERA_CLICKED,
        POPUP_ATTACH_CAMERA_ENABLED,
        POPUP_ATTACH_CAMERA_VISIBLE,
        POPUP_ATTACH_CLIPBOARD_CLICKED,
        POPUP_ATTACH_CLIPBOARD_ENABLED,
        POPUP_ATTACH_CLIPBOARD_VISIBLE,
        POPUP_ATTACH_CURRENT_TAB_CLICKED,
        POPUP_ATTACH_CURRENT_TAB_ENABLED,
        POPUP_ATTACH_CURRENT_TAB_FAVICON,
        POPUP_ATTACH_CURRENT_TAB_VISIBLE,
        POPUP_ATTACH_FILE_CLICKED,
        POPUP_ATTACH_FILE_ENABLED,
        POPUP_ATTACH_FILE_VISIBLE,
        POPUP_ATTACH_GALLERY_CLICKED,
        POPUP_ATTACH_GALLERY_ENABLED,
        POPUP_ATTACH_GALLERY_VISIBLE,
        POPUP_ATTACH_TAB_PICKER_CLICKED,
        POPUP_ATTACH_TAB_PICKER_ENABLED,
        POPUP_ATTACH_TAB_PICKER_VISIBLE,
        POPUP_TOOL_AI_MODE_CLICKED,
        POPUP_TOOL_AI_MODE_ENABLED,
        POPUP_TOOL_AI_MODE_VISIBLE,
        POPUP_TOOL_CREATE_IMAGE_CLICKED,
        POPUP_TOOL_CREATE_IMAGE_ENABLED,
        POPUP_TOOL_CREATE_IMAGE_VISIBLE,
        SHOW_DEDICATED_MODE_BUTTON
        // go/keep-sorted end
    };
}
