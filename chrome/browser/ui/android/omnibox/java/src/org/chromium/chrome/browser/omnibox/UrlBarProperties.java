// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.ActionMode;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarTextContextMenuDelegate;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Locale;

/** The model properties for the URL bar text component. */
@NullMarked
class UrlBarProperties {
    /** Contains the necessary information to update the text shown in the UrlBar. */
    static class UrlBarTextState {
        /** The text to be shown. */
        public final CharSequence text;

        /** The text for Autofill services. */
        public final CharSequence textForAutofillServices;

        /** Specifies how the text should be scrolled in the unfocused state. */
        public final @ScrollType int scrollType;

        /** Specifies the index to scroll to if {@link ScrollType#SCROLL_TO_TLD} is specified. */
        public final int scrollToIndex;

        /** Specifies how the text should be selected in the focused state. */
        public final TextSelection selection;

        /** Whether the origin has changed since the last update. */
        public final boolean originChanged;

        public UrlBarTextState(
                CharSequence text,
                CharSequence textForAutofillServices,
                @ScrollType int scrollType,
                int scrollToIndex,
                TextSelection selection,
                boolean originChanged) {
            this.text = text;
            this.textForAutofillServices = textForAutofillServices;
            this.scrollType = scrollType;
            this.scrollToIndex = scrollToIndex;
            this.selection = selection;
            this.originChanged = originChanged;
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.US,
                    "%s: text: %s; scrollType: %d; selectionState: %s",
                    getClass().getSimpleName(),
                    text,
                    scrollType,
                    selection);
        }
    }

    /** Contains the necessary information to display inline autocomplete text. */
    static class AutocompleteText {
        /** The text preceding the autocomplete text (typically entered by the user). */
        public final String userText;

        /** The inline autocomplete text to be appended to the end of the user text. */
        public final @Nullable String autocompleteText;

        /**
         * This string is displayed adjacent to the omnibox if this match is the default. Will
         * usually be a URL when autocompleting a title, and empty otherwise.
         */
        public final @Nullable String additionalText;

        /** The site search label. */
        public final @Nullable String siteSearchLabel;

        public AutocompleteText(
                String userText,
                @Nullable String autocompleteText,
                @Nullable String additionalText,
                @Nullable String siteSearchLabel) {
            this.userText = userText;
            this.autocompleteText = autocompleteText;
            this.additionalText = additionalText;
            this.siteSearchLabel = siteSearchLabel;
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.US,
                    "%s: user text: %s; autocomplete text: %s; additional text: %s; site search"
                            + " label: %s",
                    getClass().getSimpleName(),
                    userText,
                    autocompleteText,
                    additionalText,
                    siteSearchLabel);
        }
    }

    /** The string to append to the end of the URL bar text during TalkBack readout. */
    public static final WritableObjectPropertyKey<String> ACCESSIBILITY_WARNING =
            new WritableObjectPropertyKey<>();

    /** The callback for contextual action modes (cut, copy, etc.). */
    public static final ReadableObjectPropertyKey<ActionMode.Callback> ACTION_MODE_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** Whether the AI Mode preference is currently enabled. */
    public static final WritableBooleanPropertyKey AI_MODE_PREF_ENABLED =
            new WritableBooleanPropertyKey();

    /** The callback to run when the "Always Show AI Mode" menu item is toggled. */
    public static final WritableObjectPropertyKey<Callback<Boolean>> AI_MODE_PREF_TOGGLE_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** Whether focus should be allowed on the view. */
    public static final WritableBooleanPropertyKey ALLOW_FOCUS = new WritableBooleanPropertyKey();

    /** Whether multiline input should be allowed on the view. */
    public static final WritableBooleanPropertyKey ALLOW_MULTILINE_INPUT =
            new WritableBooleanPropertyKey();

    /** Specifies the autocomplete text to be shown to the user. */
    public static final WritableObjectPropertyKey<AutocompleteText> AUTOCOMPLETE_TEXT =
            new WritableObjectPropertyKey<>();

    /** The main delegate that provides additional capabilities to the UrlBar. */
    public static final ReadableObjectPropertyKey<UrlBarDelegate> DELEGATE =
            new ReadableObjectPropertyKey<>();

    /** The callback to be notified on focus changes. */
    public static final ReadableObjectPropertyKey<Callback<UrlBarFocusChangeInfo>>
            FOCUS_CHANGE_CALLBACK = new ReadableObjectPropertyKey<>();

    /** Specifies whether suggestions are showing below the URL bar. */
    public static final WritableBooleanPropertyKey HAS_URL_SUGGESTIONS =
            new WritableBooleanPropertyKey();

    /** Specifies the URL bar hint text. */
    public static final WritableObjectPropertyKey<CharSequence> HINT_TEXT =
            new WritableObjectPropertyKey<>();

    /** Specifies the color for URL bar hint text. */
    public static final WritableIntPropertyKey HINT_TEXT_COLOR = new WritableIntPropertyKey();

    /**
     * Specifies whether incognito colors should be used in the view, meaning baseline dark theme
     * without dynamic colors.
     */
    public static final WritableBooleanPropertyKey INCOGNITO_COLORS_ENABLED =
            new WritableBooleanPropertyKey();

    /** The callback to be notified on URL key events. */
    public static final ReadableObjectPropertyKey<View.OnKeyListener> KEY_DOWN_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** The handler receiving long-click events for the URL bar. */
    public static final ReadableObjectPropertyKey<View.OnLongClickListener> LONG_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** The callback to run when the "Manage search engines" menu item is clicked. */
    public static final WritableObjectPropertyKey<Runnable> MANAGE_SEARCH_ENGINES_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The callback to be notified on raw URL text changes (rich context). */
    public static final WritableObjectPropertyKey<Callback<UrlBarTextChangeInfo>>
            RICH_TEXT_CHANGE_LISTENER = new WritableObjectPropertyKey<>();

    /** Specifies whether the text should be selected when the URL bar is focused. */
    public static final WritableBooleanPropertyKey SELECT_ALL_ON_FOCUS =
            new WritableBooleanPropertyKey();

    /** Whether the hint text should be shown in the view. */
    public static final WritableBooleanPropertyKey SHOW_HINT_TEXT =
            new WritableBooleanPropertyKey();

    /** The callback to be notified on URL text changes. */
    public static final WritableObjectPropertyKey<Callback<String>> TEXT_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Specifies the color for URL bar text. */
    public static final WritableIntPropertyKey TEXT_COLOR = new WritableIntPropertyKey();

    /** The delegate that provides additional functionality to the textual context actions. */
    public static final WritableObjectPropertyKey<UrlBarTextContextMenuDelegate>
            TEXT_CONTEXT_MENU_DELEGATE = new WritableObjectPropertyKey<>();

    /** The primary text state for what is shown in the view. */
    public static final WritableObjectPropertyKey<UrlBarTextState> TEXT_STATE =
            new WritableObjectPropertyKey<>();

    /** The callback to be notified when the URL text wraps. */
    public static final ReadableObjectPropertyKey<Callback<Boolean>> TEXT_WRAPPED_CALLBACK =
            new ReadableObjectPropertyKey<>();

    /** The listener to be notified of URL direction changes. */
    public static final WritableObjectPropertyKey<Callback<Integer>> URL_DIRECTION_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Whether the URL bar should use a small text size. */
    public static final WritableBooleanPropertyKey USE_SMALL_TEXT =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                // go/keep-sorted start
                ACCESSIBILITY_WARNING,
                ACTION_MODE_CALLBACK,
                AI_MODE_PREF_ENABLED,
                AI_MODE_PREF_TOGGLE_CALLBACK,
                ALLOW_FOCUS,
                ALLOW_MULTILINE_INPUT,
                AUTOCOMPLETE_TEXT,
                DELEGATE,
                FOCUS_CHANGE_CALLBACK,
                HAS_URL_SUGGESTIONS,
                HINT_TEXT,
                HINT_TEXT_COLOR,
                INCOGNITO_COLORS_ENABLED,
                KEY_DOWN_LISTENER,
                LONG_CLICK_LISTENER,
                MANAGE_SEARCH_ENGINES_CALLBACK,
                RICH_TEXT_CHANGE_LISTENER,
                SELECT_ALL_ON_FOCUS,
                SHOW_HINT_TEXT,
                TEXT_CHANGE_LISTENER,
                TEXT_COLOR,
                TEXT_CONTEXT_MENU_DELEGATE,
                TEXT_STATE,
                TEXT_WRAPPED_CALLBACK,
                URL_DIRECTION_LISTENER,
                USE_SMALL_TEXT
                // go/keep-sorted end
            };
}
