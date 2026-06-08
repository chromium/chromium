// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.PopupState;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** The properties associated with the Fusebox bar. */
@NullMarked
class FuseboxProperties {
    @IntDef({PopupButtonType.RECENT_TAB, PopupButtonType.TOOL, PopupButtonType.MODEL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PopupButtonType {
        int RECENT_TAB = 0;
        int TOOL = 1;
        int MODEL = 2;
    }

    /** Encapsulates the state for a button in the Fusebox popup. */
    public static class PopupButtonData {
        public final Runnable onClicked;
        public final String text;
        // Either iconId (predefined vector drawable) or customIcon (bitmap favicon) is set.
        public final /*IconResourceIds*/ int iconId;
        public final @Nullable Bitmap customIcon;
        public final boolean enabled;
        public final boolean selected;
        public final @PopupButtonType int type;
        public final int protoId;
        public final boolean hasColor;

        public PopupButtonData(
                Callback<PopupButtonData> onClicked,
                String text,
                int iconId,
                boolean enabled,
                boolean selected,
                @PopupButtonType int type,
                int protoId,
                boolean hasColor) {
            this.onClicked = onClicked.bind(this);
            this.text = text;
            this.iconId = iconId;
            this.customIcon = null;
            this.enabled = enabled;
            this.selected = selected;
            this.type = type;
            this.protoId = protoId;
            this.hasColor = hasColor;
        }

        public PopupButtonData(
                Callback<PopupButtonData> onClicked,
                String text,
                @Nullable Bitmap customIcon,
                boolean enabled,
                boolean selected,
                @PopupButtonType int type,
                int protoId,
                boolean hasColor) {
            this.onClicked = onClicked.bind(this);
            this.text = text;
            this.iconId = 0;
            this.customIcon = customIcon;
            this.enabled = enabled;
            this.selected = selected;
            this.type = type;
            this.protoId = protoId;
            this.hasColor = hasColor;
        }
    }

    /** Action to perform when the user clicks the activation chip. */
    public static final WritableObjectPropertyKey<Runnable> ACTIVATION_CHIP_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the activation chip should be selected. */
    public static final WritableBooleanPropertyKey ACTIVATION_CHIP_SELECTED =
            new WritableBooleanPropertyKey();

    /** Whether the activation chip should be visible. */
    public static final WritableBooleanPropertyKey ACTIVATION_CHIP_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The adapter for the attachments RecyclerView. */
    public static final WritableObjectPropertyKey<SimpleRecyclerViewAdapter> ADAPTER =
            new WritableObjectPropertyKey<>();

    /** Whether the attachments RecyclerView is visible. */
    public static final WritableBooleanPropertyKey ATTACHMENTS_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The variant of {@link BrandedColorScheme} to apply to the UI elements. */
    public static final WritableObjectPropertyKey<@BrandedColorScheme Integer> COLOR_SCHEME =
            new WritableObjectPropertyKey<>();

    /** The layout mode of fusebox views; see {@link FuseboxLayoutMode}. */
    public static final WritableObjectPropertyKey<@FuseboxLayoutMode Integer> FUSEBOX_LAYOUT_MODE =
            new WritableObjectPropertyKey<>();

    /** The state of the UI of the fusebox should currently be in. */
    public static final WritableObjectPropertyKey<@FuseboxState Integer> FUSEBOX_STATE =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the Plus button. */
    public static final WritableObjectPropertyKey<Runnable> PLUS_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the plus button is visible. */
    public static final WritableBooleanPropertyKey PLUS_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Action to perform when the user clicks the Camera button in the popup. */
    public static final WritableObjectPropertyKey<Runnable> POPUP_ATTACH_CAMERA_CLICKED =
            new WritableObjectPropertyKey<>();

    /** Whether the Camera button in the popup is enabled. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CAMERA_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the Camera button in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_ATTACH_CAMERA_VISIBLE =
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

    /** Holds button data objects for each model that is to be shown. */
    public static final WritableObjectPropertyKey<List<PopupButtonData>>
            POPUP_MODEL_BUTTON_DATA_LIST = new WritableObjectPropertyKey<>();

    /** Whether the models divider in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_MODEL_DIVIDER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The text for the models header in the popup. */
    public static final WritableObjectPropertyKey<String> POPUP_MODEL_HEADER_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether the models header in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_MODEL_HEADER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Holds button data objects for each recent tab that is to be shown. */
    public static final WritableObjectPropertyKey<List<PopupButtonData>>
            POPUP_RECENT_TABS_BUTTON_DATA_LIST = new WritableObjectPropertyKey<>();

    /** Whether the recent tabs divider in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_RECENT_TABS_DIVIDER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the recent tab buttons in the popup are enabled. */
    public static final WritableBooleanPropertyKey POPUP_RECENT_TABS_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the recent tabs header in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_RECENT_TABS_HEADER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The state of the popup. */
    public static final WritableObjectPropertyKey<@PopupState Integer> POPUP_STATE =
            new WritableObjectPropertyKey<>();

    /** Holds button data objects for each tool that is to be shown. */
    public static final WritableObjectPropertyKey<List<PopupButtonData>>
            POPUP_TOOL_BUTTON_DATA_LIST = new WritableObjectPropertyKey<>();

    /** Whether the tools divider in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_DIVIDER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** The text for the tools header in the popup. */
    public static final WritableObjectPropertyKey<String> POPUP_TOOL_HEADER_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether the tools header in the popup is visible. */
    public static final WritableBooleanPropertyKey POPUP_TOOL_HEADER_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Tracks the {@link AutocompleteRequestType}. */
    public static final WritableObjectPropertyKey<@AutocompleteRequestType Integer> REQUEST_TYPE =
            new WritableObjectPropertyKey<>();

    /** Action to perform when the user clicks the request type button. */
    public static final WritableObjectPropertyKey<Runnable> REQUEST_TYPE_BUTTON_CLICKED =
            new WritableObjectPropertyKey<>();

    /** The text for the request type button. */
    public static final WritableObjectPropertyKey<String> REQUEST_TYPE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Whether the request type button is visible. */
    public static final WritableBooleanPropertyKey REQUEST_TYPE_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        // go/keep-sorted start
        ACTIVATION_CHIP_CLICKED,
        ACTIVATION_CHIP_SELECTED,
        ACTIVATION_CHIP_VISIBLE,
        ADAPTER,
        ATTACHMENTS_VISIBLE,
        COLOR_SCHEME,
        FUSEBOX_LAYOUT_MODE,
        FUSEBOX_STATE,
        PLUS_BUTTON_CLICKED,
        PLUS_BUTTON_VISIBLE,
        POPUP_ATTACH_CAMERA_CLICKED,
        POPUP_ATTACH_CAMERA_ENABLED,
        POPUP_ATTACH_CAMERA_VISIBLE,
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
        POPUP_MODEL_BUTTON_DATA_LIST,
        POPUP_MODEL_DIVIDER_VISIBLE,
        POPUP_MODEL_HEADER_TEXT,
        POPUP_MODEL_HEADER_VISIBLE,
        POPUP_RECENT_TABS_BUTTON_DATA_LIST,
        POPUP_RECENT_TABS_DIVIDER_VISIBLE,
        POPUP_RECENT_TABS_ENABLED,
        POPUP_RECENT_TABS_HEADER_VISIBLE,
        POPUP_STATE,
        POPUP_TOOL_BUTTON_DATA_LIST,
        POPUP_TOOL_DIVIDER_VISIBLE,
        POPUP_TOOL_HEADER_TEXT,
        POPUP_TOOL_HEADER_VISIBLE,
        REQUEST_TYPE,
        REQUEST_TYPE_BUTTON_CLICKED,
        REQUEST_TYPE_BUTTON_TEXT,
        REQUEST_TYPE_BUTTON_VISIBLE
        // go/keep-sorted end
    };
}
