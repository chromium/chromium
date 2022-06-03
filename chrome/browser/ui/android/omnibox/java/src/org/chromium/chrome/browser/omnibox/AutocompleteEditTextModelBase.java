// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.Editable;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.InputConnection;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

/**
 * An abstraction of the text model to show, keep track of, and update autocomplete.
 */
public interface AutocompleteEditTextModelBase {
    /**
     * An embedder should implement this.
     */
    public interface Delegate {
        /** @see TextView#getText() */
        Editable getText();
        /** @see TextView#getEditableText() */
        Editable getEditableText();
        /** @see TextView#append(CharSequence) */
        void append(CharSequence subSequence);
        /** @see TextView#getSelectionStart() */
        int getSelectionStart();
        /** @see TextView#getSelectionEnd() */
        int getSelectionEnd();
        /** @see EditText#setSelection(int, int) */
        void setSelection(int autocompleteIndex, int length);
        /** @see TextView#announceForAccessibility(CharSequence) */
        void announceForAccessibility(CharSequence inlineAutocompleteText);
        /** @see TextView#getHighlightColor() */
        int getHighlightColor();
        /** @see TextView#setCursorVisible(boolean) */
        void setCursorVisible(boolean visible);
        /** @see TextView#isFocused() */
        boolean isFocused();
        /** @see TextView#sendAccessibilityEventUnchecked(AccessibilityEvent) */
        void sendAccessibilityEventUnchecked(AccessibilityEvent event);

        /**
         * Call super.dispatchKeyEvent(KeyEvent).
         * @param event Key event.
         * @return The return value of super.dispatchKeyEvent(KeyEvent).
         */
        boolean super_dispatchKeyEvent(KeyEvent event);

        /**
         * This is called when autocomplete replaces the whole text.
         * @param text The text.
         */
        void replaceAllTextFromAutocomplete(String text);

        /** @return Whether accessibility is enabled. */
        boolean isAccessibilityEnabled();

        /**
         * This is called when autocomplete text state changes.
         * @param updateDisplay True if string is changed.
         */
        void onAutocompleteTextStateChanged(boolean updateDisplay);

        /**
         * This is called roughly the same time as when we call
         * InputMethodManager#updateSelection().
         *
         * @param selStart Selection start.
         * @param selEnd Selection end.
         */
        void onUpdateSelectionForTesting(int selStart, int selEnd);

        /** @return The package name of the current keyboard app. */
        String getKeyboardPackageName();
    }

    /**
     * Called when creating an input connection.
     * @param inputConnection An {@link InputConnection} created by EditText.
     * @return A wrapper @{link InputConnection} created by the model.
     */
    InputConnection onCreateInputConnection(InputConnection inputConnection);

    /**
     * Called when View#dispatchKeyEvent(KeyEvent event) is called.
     * @param event The key event.
     * @return True if key event has been handled, false otherwise.
     */
    boolean dispatchKeyEvent(KeyEvent event);

    /**
     * Called when TextView#setText(CharSequence, BufferType) is called.
     * @param text The new text.
     */
    void onSetText(CharSequence text);

    /**
     * Called when TextView#onSelectionChanged(int, int) is called.
     * @param selStart The selection start.
     * @param selEnd The selection end.
     */
    void onSelectionChanged(int selStart, int selEnd);

    /**
     * Called when View#onFocusChanged(boolean, int, Rect) is called.
     * @param focused True if the View has focus; false otherwise.
     */
    void onFocusChanged(boolean focused);

    /**
     * Called when TextView#onTextChanged(CharSequence, int, int, int) is called.
     * @param text The text the TextView is displaying.
     * @param start The offset of the start of the range of the text that was modified.
     * @param beforeLength The length of the former text that has been replaced.
     * @param afterLength The length of the replacement modified text.
     */
    void onTextChanged(CharSequence text, int start, int beforeLength, int afterLength);

    /** Called when text gets pasted. */
    void onPaste();

    /**
     * @return The whole text including both user text and autocomplete text.
     */
    String getTextWithAutocomplete();

    /**
     * @return The user text without the autocomplete text.
     */
    String getTextWithoutAutocomplete();

    /**
     * Returns the length of the autocomplete text currently displayed, zero if none is
     * currently displayed.
     */
    String getAutocompleteText();

    /**
     * Sets whether text changes should trigger autocomplete.
     * @param ignore Whether text changes should be ignored and no auto complete.
     */
    void setIgnoreTextChangeFromAutocomplete(boolean ignore);

    /**
     * Autocompletes the text and selects the text that was not entered by the user. Using append()
     * instead of setText() to preserve the soft-keyboard layout.
     * @param userText user The text entered by the user.
     * @param inlineAutocompleteText The suggested autocompletion for the user's text.
     */
    void setAutocompleteText(CharSequence userText, CharSequence inlineAutocompleteText);

    /**
     * Whether we want to be showing inline autocomplete results. We don't want to show them as the
     * user deletes input. Also if there is a composition (e.g. while using the Japanese IME),
     * we must not autocomplete or we'll destroy the composition.
     * @return Whether we want to be showing inline autocomplete results.
     */
    boolean shouldAutocomplete();

    /** @return Whether any autocomplete information is specified on the current text. */
    @VisibleForTesting
    boolean hasAutocomplete();

    /** @return The current {@link InputConnection} object. */
    @VisibleForTesting
    InputConnection getInputConnection();

    /**
     * @return Whether accessibility event should be ignored.
     */
    boolean shouldIgnoreAccessibilityEvent();
}
