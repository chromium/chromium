// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.graphics.Typeface;
import android.view.ActionMode;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarTextContextMenuDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.omnibox.UrlBarCoordinator.SelectionState;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Locale;

/**
 * The model properties for the URL bar text component.
 */
class UrlBarProperties {
    /**
     * Contains the necessary information to update the text shown in the UrlBar.
     */
    static class UrlBarTextState {
        /** Text to be shown. */
        public final CharSequence text;

        /** Text for Autofill services. */
        public final CharSequence textForAutofillServices;

        /** Specifies how the text should be scrolled in the unfocused state. */
        public final @ScrollType int scrollType;

        /** Specifies the index to scroll to if {@link UrlBar#SCROLL_TO_TLD} is specified. */
        public int scrollToIndex;

        /** Specifies how the text should be selected in the focused state. */
        public final @SelectionState int selectionState;

        public UrlBarTextState(CharSequence text, CharSequence textForAutofillServices,
                @ScrollType int scrollType, int scrollToIndex, @SelectionState int selectionState) {
            this.text = text;
            this.textForAutofillServices = textForAutofillServices;
            this.scrollType = scrollType;
            this.scrollToIndex = scrollToIndex;
            this.selectionState = selectionState;
        }

        @Override
        public String toString() {
            return String.format(Locale.US, "%s: text: %s; scrollType: %d; selectionState: %d",
                    getClass().getSimpleName(), text, scrollType, selectionState);
        }
    }

    /**
     * Contains the necessary information to display inline autocomplete text.
     */
    static class AutocompleteText {
        /** The text preceding the autocomplete text (typically entered by the user). */
        public final String userText;

        /** The inline autocomplete text to be appended to the end of the user text. */
        public final String autocompleteText;

        public AutocompleteText(String userText, String autocompleteText) {
            this.userText = userText;
            this.autocompleteText = autocompleteText;
        }

        @Override
        public String toString() {
            return String.format(Locale.US, "%s: user text: %s; autocomplete text: %s",
                    getClass().getSimpleName(), userText, autocompleteText);
        }
    }

    /** The callback for contextual action modes (cut, copy, etc...). */
    public static final WritableObjectPropertyKey<ActionMode.Callback> ACTION_MODE_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** Whether focus should be allowed on the view. */
    public static final WritableBooleanPropertyKey ALLOW_FOCUS = new WritableBooleanPropertyKey();

    /** Specified the autocomplete text to be shown to the user. */
    public static final WritableObjectPropertyKey<AutocompleteText> AUTOCOMPLETE_TEXT =
            new WritableObjectPropertyKey<>();

    /** The main delegate that provides additional capabilities to the UrlBar. */
    public static final WritableObjectPropertyKey<UrlBarDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** The callback to be notified on focus changes. */
    public static final WritableObjectPropertyKey<Callback<Boolean>> FOCUS_CHANGE_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** Whether the cursor should be shown in the view. */
    public static final WritableBooleanPropertyKey SHOW_CURSOR = new WritableBooleanPropertyKey();

    /** Delegate that provides additional functionality to the textual context actions. */
    public static final WritableObjectPropertyKey<UrlBarTextContextMenuDelegate>
            TEXT_CONTEXT_MENU_DELEGATE = new WritableObjectPropertyKey<>();

    /** The primary text state for what is shown in the view. */
    public static final WritableObjectPropertyKey<UrlBarTextState> TEXT_STATE =
            new WritableObjectPropertyKey<>();

    /** The listener to be notified of URL direction changes. */
    public static final WritableObjectPropertyKey<Callback<Integer>> URL_DIRECTION_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The callback to be notified on url text changes. @see UrlTextChangeListener. */
    public static final WritableObjectPropertyKey<UrlTextChangeListener> URL_TEXT_CHANGE_LISTENER =
            new WritableObjectPropertyKey<>();

    /**
     * Specifies the typeface for url bar text.
     */
    public static final WritableObjectPropertyKey<Typeface> TYPEFACE =
            new WritableObjectPropertyKey<>();

    /**
     * Specifies the color for url bar text.
     */
    public static final WritableIntPropertyKey TEXT_COLOR = new WritableIntPropertyKey();

    /**
     * Specifies the color for url bar hint text.
     */
    public static final WritableIntPropertyKey HINT_TEXT_COLOR = new WritableIntPropertyKey();

    /**
     * Specifies whether incognito colors should be used in the view, meaning baseline dark theme
     *  without dynamic colors.
     */
    public static final WritableBooleanPropertyKey INCOGNITO_COLORS_ENABLED =
            new WritableBooleanPropertyKey();

    /** The delegate that provides Window capabilities to the view. */
    public static final WritableObjectPropertyKey<WindowDelegate> WINDOW_DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Specifies whether suggestions are showing below the URL bar. */
    public static final WritableBooleanPropertyKey HAS_URL_SUGGESTIONS =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ACTION_MODE_CALLBACK, ALLOW_FOCUS, AUTOCOMPLETE_TEXT, DELEGATE,
                    FOCUS_CHANGE_CALLBACK, SHOW_CURSOR, TEXT_CONTEXT_MENU_DELEGATE, TEXT_STATE,
                    URL_DIRECTION_LISTENER, URL_TEXT_CHANGE_LISTENER, INCOGNITO_COLORS_ENABLED,
                    WINDOW_DELEGATE, HAS_URL_SUGGESTIONS, TYPEFACE, TEXT_COLOR, HINT_TEXT_COLOR};
}
