// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.ForegroundColorSpan;
import android.view.inputmethod.BaseInputConnection;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.OmniboxFeatures;

import java.util.Locale;

/**
 * A class to set and remove, or do other operations on Span and SpannableString of autocomplete
 * text that will be appended to the user text. In addition, cursor will be hidden whenever we are
 * showing span to the user.
 */
@NullMarked
class SpanCursorController {
    private static final String TAG = "SpanCursorController";
    private static final boolean DEBUG = OmniboxFeatures.sDiagInputConnection.getValue();

    private final AutocompleteEditTextModelBase.Delegate mDelegate;
    private final BackgroundColorSpan mAutocompleteBgColorSpan;
    private final ForegroundColorSpan mAdditionalTextFgColorSpan;

    public SpanCursorController(AutocompleteEditTextModelBase.Delegate delegate, Context context) {
        mDelegate = delegate;
        mAutocompleteBgColorSpan = new BackgroundColorSpan(mDelegate.getHighlightColor());
        mAdditionalTextFgColorSpan =
                new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorSecondary(context));
    }

    public void setSpan(AutocompleteState state) {
        int sel = state.getSelection().from;

        // Build the whole suffix (autocomplete text + optional additional text) up front and
        // append it in a single Editable mutation. Every mutation of the Editable forces the
        // TextView's DynamicLayout to regenerate the StaticLayout for the full (single-line)
        // text, so two appends cost two full relayouts. See b/576773721.
        SpannableStringBuilder suffix = new SpannableStringBuilder();
        String autocompleteText = state.getAutocompleteText();
        if (autocompleteText != null) {
            // The flag here helps make sure that span does not get spill to other part of the
            // text.
            suffix.append(
                    autocompleteText, mAutocompleteBgColorSpan, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        String additionalTextStr = state.getAdditionalText();
        if (additionalTextStr != null
                && OmniboxFeatures.shouldShowRichInlineAutocompleteUrl(
                        state.getUserText().length())) {
            suffix.append(
                    " - " + additionalTextStr,
                    mAdditionalTextFgColorSpan,
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        Editable editable = mDelegate.getEditableText();
        if (suffix.length() > 0) {
            try (var capture = new UndoStackCapture(editable)) {
                editable.append(suffix);
            }
        }

        // Keep the original selection before adding spannable string.
        int clampedSel = Math.min(sel, editable.length());
        Selection.setSelection(editable, clampedSel, clampedSel);
        setCursorVisible(false);
        if (DEBUG) Log.i(TAG, "setSpan: " + toDebugString(editable));
    }

    private void setCursorVisible(boolean visible) {
        if (mDelegate.isFocused()) mDelegate.setCursorVisible(visible);
    }

    /**
     * Re-applies the cursor visibility that {@link #setSpan} establishes while autocomplete text is
     * shown, without touching the {@link Editable}.
     */
    public void hideCursorForSpan() {
        setCursorVisible(false);
    }

    /**
     * @return Whether the autocomplete span is currently attached to the {@link Editable}.
     */
    public boolean hasAutocompleteSpan() {
        return getAutocompleteSpanIndex(mDelegate.getEditableText()) != -1;
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
        try (var capture = new UndoStackCapture(editable)) {
            editable.removeSpan(span);
            editable.delete(idx, editable.length());
        }
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
     * @return Debug string for the given {@link Editable}.
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
