// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.ForegroundColorSpan;
import android.view.inputmethod.BaseInputConnection;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.components.omnibox.OmniboxFeatures;

import java.util.Locale;

/**
 * A class to set and remove, or do other operations on Span and SpannableString of autocomplete
 * text that will be appended to the user text. In addition, cursor will be hidden whenever we are
 * showing span to the user.
 */
class SpanCursorController {
    private static final String TAG = "SpanCursorController";
    private static final boolean DEBUG = false;

    private final @NonNull AutocompleteEditTextModelBase.Delegate mDelegate;
    private final @NonNull BackgroundColorSpan mAutocompleteBgColorSpan;
    private final @NonNull ForegroundColorSpan mAdditionalTextFgColorSpan;

    public SpanCursorController(AutocompleteEditTextModelBase.Delegate delegate, Context context) {
        mDelegate = delegate;
        mAutocompleteBgColorSpan = new BackgroundColorSpan(mDelegate.getHighlightColor());
        mAdditionalTextFgColorSpan =
                new ForegroundColorSpan(OmniboxResourceProvider.getAdditionalTextColor(context));
    }

    public void setSpan(AutocompleteState state) {
        int sel = state.getSelStart();

        Editable editable = mDelegate.getEditableText();

        if (state.getAutocompleteText().isPresent()) {
            SpannableString spanString = new SpannableString(state.getAutocompleteText().get());
            // The flag here helps make sure that span does not get spill to other part of the
            // text.
            spanString.setSpan(
                    mAutocompleteBgColorSpan,
                    0,
                    state.getAutocompleteText().map(t -> t.length()).orElse(0),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            editable.append(spanString);
        }

        if (state.getAdditionalText().isPresent()
                && OmniboxFeatures.shouldShowRichInlineAutocompleteUrl(
                        state.getUserText().length())) {
            String additionalText = " - " + state.getAdditionalText().get();
            SpannableString additionalTextSpanString = new SpannableString(additionalText);
            additionalTextSpanString.setSpan(
                    mAdditionalTextFgColorSpan,
                    0,
                    additionalText.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            editable.append(additionalTextSpanString);
        }

        // Keep the original selection before adding spannable string.
        Selection.setSelection(editable, sel, sel);
        setCursorVisible(false);
        if (DEBUG) Log.i(TAG, "setSpan: " + toDebugString(editable));
    }

    private void setCursorVisible(boolean visible) {
        if (mDelegate.isFocused()) mDelegate.setCursorVisible(visible);
    }

    private int getAutocompleteSpanIndex(Editable editable) {
        return getSpanIndex(editable, mAutocompleteBgColorSpan);
    }

    private int getSpanIndex(Editable editable, CharacterStyle span) {
        if (editable == null) return -1;
        // returns -1 if span is not attached
        return editable.getSpanStart(span);
    }

    public void reset() {
        setCursorVisible(true);
        Editable editable = mDelegate.getEditableText();
        int idx = getAutocompleteSpanIndex(editable);
        if (idx != -1) {
            editable.removeSpan(mAutocompleteBgColorSpan);
        }
    }

    /**
     * Remove the autocomplete span. If the additional text span is available, the additional text
     * span will be removed as well.
     *
     * @return {@code true} if the span is found and deleted. {@code false} otherwise.
     */
    public boolean removeAutocompleteSpan() {
        return removeSpan(mAutocompleteBgColorSpan);
    }

    /**
     * Remove the additional text span.
     *
     * @return {@code true} if the span is found and deleted. {@code false} otherwise.
     */
    public boolean removeAdditionalTextSpan() {
        return removeSpan(mAdditionalTextFgColorSpan);
    }

    /**
     * Remove the span.
     *
     * @param span The span to be removed.
     * @return {@code true} if the span is found and deleted. {@code false} otherwise.
     */
    private boolean removeSpan(CharacterStyle span) {
        setCursorVisible(true);
        Editable editable = mDelegate.getEditableText();
        int idx = getSpanIndex(editable, span);
        if (idx == -1) return false;
        if (DEBUG) Log.i(TAG, "removeSpan IDX[%d]", idx);
        editable.removeSpan(span);
        editable.delete(idx, editable.length());
        if (DEBUG) {
            Log.i(TAG, "removeSpan - after removal: " + toDebugString(editable));
        }
        return true;
    }

    public void commitSpan() {
        mDelegate.getEditableText().removeSpan(mAutocompleteBgColorSpan);
        // Remove the additional text when autocomplete is committed.
        removeAdditionalTextSpan();
        setCursorVisible(true);
    }

    public void reflectTextUpdateInState(AutocompleteState state, CharSequence text) {
        if (text instanceof Editable) {
            Editable editable = (Editable) text;
            int idx = getAutocompleteSpanIndex(editable);
            if (idx != -1) {
                // We do not set autocomplete text here as model should solely control it.
                state.setUserText(editable.subSequence(0, idx).toString());
                return;
            }
        }
        state.setUserText(text.toString());
    }

    /**
     * @param editable The editable.
     * @return Debug string for the given {@Editable}.
     */
    private static String toDebugString(Editable editable) {
        return String.format(
                Locale.US,
                "Editable {[%s] SEL[%d %d] COM[%d %d]}",
                editable.toString(),
                Selection.getSelectionStart(editable),
                Selection.getSelectionEnd(editable),
                BaseInputConnection.getComposingSpanStart(editable),
                BaseInputConnection.getComposingSpanEnd(editable));
    }
}
