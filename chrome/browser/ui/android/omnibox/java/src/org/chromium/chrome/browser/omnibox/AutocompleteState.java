// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.util.Locale;
import java.util.Optional;

/** A state to keep track of EditText and autocomplete. */
class AutocompleteState {
    @NonNull private String mUserText;
    @NonNull private Optional<String> mAutocompleteText;
    @NonNull private Optional<String> mAdditionalText;
    private int mSelStart;
    private int mSelEnd;

    public AutocompleteState(AutocompleteState a) {
        copyFrom(a);
    }

    public AutocompleteState(
            @NonNull String userText,
            @Nullable String autocompleteText,
            @Nullable String additionalText,
            int selStart,
            int selEnd) {
        set(
                userText,
                TextUtils.isEmpty(autocompleteText)
                        ? Optional.empty()
                        : Optional.of(autocompleteText),
                TextUtils.isEmpty(additionalText) ? Optional.empty() : Optional.of(additionalText),
                selStart,
                selEnd);
    }

    public void set(
            @NonNull String userText,
            Optional<String> autocompleteText,
            Optional<String> additionalText,
            int selStart,
            int selEnd) {
        mUserText = userText;
        mAutocompleteText = autocompleteText;
        mAdditionalText = additionalText;
        mSelStart = selStart;
        mSelEnd = selEnd;
    }

    public void copyFrom(AutocompleteState a) {
        set(a.mUserText, a.mAutocompleteText, a.mAdditionalText, a.mSelStart, a.mSelEnd);
    }

    @NonNull
    public String getUserText() {
        return mUserText;
    }

    public Optional<String> getAutocompleteText() {
        return mAutocompleteText;
    }

    public Optional<String> getAdditionalText() {
        return mAdditionalText;
    }

    /**
     * @return The whole text including autocomplete text.
     */
    @NonNull
    public String getText() {
        return TextUtils.concat(mUserText, mAutocompleteText.orElse("")).toString();
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

    public void setAutocompleteText(Optional<String> autocompleteText) {
        mAutocompleteText = autocompleteText;
    }

    public void clearAutocompleteText() {
        mAutocompleteText = Optional.empty();
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
        return isCursorAtEndOfUserText()
                && prevState.isCursorAtEndOfUserText()
                && isPrefix(mUserText, prevState.mUserText);
    }

    /**
     * @param prevState The previous state to compare the current state with.
     * @return Whether the current state is forward-typed from prevState.
     */
    public boolean isForwardTypedFrom(AutocompleteState prevState) {
        return isCursorAtEndOfUserText()
                && prevState.isCursorAtEndOfUserText()
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
     *
     * @param prevState The previous state.
     * @return Whether the shifting was successful.
     */
    public boolean reuseAutocompleteTextIfPrefixExtension(AutocompleteState prevState) {
        // Shift when user text has grown or remains the same, but still prefix of prevState's whole
        // text.
        int diff = mUserText.length() - prevState.mUserText.length();
        if (diff < 0) return false;
        if (!isPrefix(mUserText, prevState.getText())) return false;
        mAutocompleteText = prevState.getAutocompleteText().map(s -> s.substring(diff));
        mAdditionalText = prevState.mAdditionalText;
        return true;
    }

    public void commitAutocompleteText() {
        mAutocompleteText.ifPresent(s -> mUserText += s);
        mAutocompleteText = Optional.empty();
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof AutocompleteState)) return false;
        if (o == this) return true;
        AutocompleteState a = (AutocompleteState) o;
        return mUserText.equals(a.mUserText)
                && mAutocompleteText.equals(a.mAutocompleteText)
                && mSelStart == a.mSelStart
                && mSelEnd == a.mSelEnd;
    }

    @Override
    public int hashCode() {
        return mUserText.hashCode() * 2
                + mAutocompleteText.map(s -> s.hashCode()).orElse(0) * 3
                + mSelStart * 5
                + mSelEnd * 7;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US,
                "AutocompleteState {[%s][%s] [%d-%d]}",
                mUserText,
                mAutocompleteText,
                mSelStart,
                mSelEnd);
    }
}
