// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import java.util.Locale;

/**
 * A state to keep track of EditText and autocomplete.
 */
class AutocompleteState {
    private String mUserText;
    private String mAutocompleteText;
    private int mSelStart;
    private int mSelEnd;

    public AutocompleteState(AutocompleteState a) {
        copyFrom(a);
    }

    public AutocompleteState(String userText, String autocompleteText, int selStart, int selEnd) {
        set(userText, autocompleteText, selStart, selEnd);
    }

    public void set(String userText, String autocompleteText, int selStart, int selEnd) {
        mUserText = userText;
        mAutocompleteText = autocompleteText;
        mSelStart = selStart;
        mSelEnd = selEnd;
    }

    public void copyFrom(AutocompleteState a) {
        set(a.mUserText, a.mAutocompleteText, a.mSelStart, a.mSelEnd);
    }

    public String getUserText() {
        return mUserText;
    }

    public String getAutocompleteText() {
        return mAutocompleteText;
    }

    public boolean hasAutocompleteText() {
        return !TextUtils.isEmpty(mAutocompleteText);
    }

    /** @return The whole text including autocomplete text. */
    public String getText() {
        return mUserText + mAutocompleteText;
    }

    public int getSelStart() {
        return mSelStart;
    }

    public int getSelEnd() {
        return mSelEnd;
    }

    public void setSelection(int selStart, int selEnd) {
        mSelStart = selStart;
        mSelEnd = selEnd;
    }

    public void setUserText(String userText) {
        mUserText = userText;
    }

    public void setAutocompleteText(String autocompleteText) {
        mAutocompleteText = autocompleteText;
    }

    public void clearAutocompleteText() {
        mAutocompleteText = "";
    }

    public boolean isCursorAtEndOfUserText() {
        return mSelStart == mUserText.length() && mSelEnd == mUserText.length();
    }

    public boolean isWholeUserTextSelected() {
        return mSelStart == 0 && mSelEnd == mUserText.length();
    }

    /**
     * @param prevState The previous state to compare the current state with.
     * @return Whether the current state is backward-deleted from prevState.
     */
    public boolean isBackwardDeletedFrom(AutocompleteState prevState) {
        return isCursorAtEndOfUserText() && prevState.isCursorAtEndOfUserText()
                && isPrefix(mUserText, prevState.mUserText);
    }

    /**
     * @param prevState The previous state to compare the current state with.
     * @return Whether the current state is forward-typed from prevState.
     */
    public boolean isForwardTypedFrom(AutocompleteState prevState) {
        return isCursorAtEndOfUserText() && prevState.isCursorAtEndOfUserText()
                && isPrefix(prevState.mUserText, mUserText);
    }

    /**
     * @param prevState The previous state to compare the current state with.
     * @return The differential string that has been backward deleted.
     */
    public String getBackwardDeletedTextFrom(AutocompleteState prevState) {
        if (!isBackwardDeletedFrom(prevState)) return null;
        return prevState.mUserText.substring(mUserText.length());
    }

    @VisibleForTesting
    public static boolean isPrefix(String a, String b) {
        return b.startsWith(a) && b.length() > a.length();
    }

    /**
     * When the user manually types the next character that was already suggested in the previous
     * autocomplete, then the suggestion is still valid if we simply remove one character from the
     * beginning of it. For example, if prev = "a[bc]" and current text is "ab", this method
     * constructs "ab[c]".
     * @param prevState The previous state.
     * @return Whether the shifting was successful.
     */
    public boolean reuseAutocompleteTextIfPrefixExtension(AutocompleteState prevState) {
        // Shift when user text has grown or remains the same, but still prefix of prevState's whole
        // text.
        int diff = mUserText.length() - prevState.mUserText.length();
        if (diff < 0) return false;
        if (!isPrefix(mUserText, prevState.getText())) return false;
        mAutocompleteText = prevState.mAutocompleteText.substring(diff);
        return true;
    }

    public void commitAutocompleteText() {
        mUserText += mAutocompleteText;
        mAutocompleteText = "";
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof AutocompleteState)) return false;
        if (o == this) return true;
        AutocompleteState a = (AutocompleteState) o;
        return mUserText.equals(a.mUserText) && mAutocompleteText.equals(a.mAutocompleteText)
                && mSelStart == a.mSelStart && mSelEnd == a.mSelEnd;
    }

    @Override
    public int hashCode() {
        return mUserText.hashCode() * 2 + mAutocompleteText.hashCode() * 3 + mSelStart * 5
                + mSelEnd * 7;
    }

    @Override
    public String toString() {
        return String.format(Locale.US, "AutocompleteState {[%s][%s] [%d-%d]}", mUserText,
                mAutocompleteText, mSelStart, mSelEnd);
    }
}