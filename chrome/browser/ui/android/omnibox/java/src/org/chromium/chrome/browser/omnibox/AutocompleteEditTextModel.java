// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.text.Editable;
import android.text.Selection;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;

import org.chromium.base.Log;

/**
 * An autocomplete model that appends autocomplete text at the end of query/URL text and selects it.
 */
public class AutocompleteEditTextModel implements AutocompleteEditTextModelBase {
    private static final String TAG = "AutocompleteModel";

    private static final boolean DEBUG = false;

    private final Delegate mDelegate;
    private final AutocompleteSpan mAutocompleteSpan;

    private int mBatchEditNestCount;
    private int mBeforeBatchEditAutocompleteIndex = -1;
    private String mBeforeBatchEditFullText;
    private boolean mSelectionChangedInBatchMode;
    private boolean mTextDeletedInBatchMode;
    // Set to true when the text is modified programmatically. Initially set to true until the old
    // state has been loaded.
    private boolean mIgnoreTextChangeFromAutocomplete = true;

    private boolean mLastEditWasDelete;
    private boolean mLastEditWasPaste;

    // For testing.
    private int mLastUpdateSelStart;
    private int mLastUpdateSelEnd;

    public AutocompleteEditTextModel(AutocompleteEditTextModel.Delegate delegate) {
        if (DEBUG) Log.i(TAG, "constructor");
        mDelegate = delegate;
        mAutocompleteSpan = new AutocompleteSpan();
    }

    @Override
    public void setIgnoreTextChangeFromAutocomplete(boolean ignore) {
        if (DEBUG) Log.i(TAG, "setIgnoreTextChangesForAutocomplete: " + ignore);
        mIgnoreTextChangeFromAutocomplete = ignore;
    }

    @Override
    public String getTextWithAutocomplete() {
        return mDelegate.getText().toString();
    }

    /**
     * @return Whether the current cursor position is at the end of the user typed text (i.e.
     *         at the beginning of the inline autocomplete text if present otherwise the very
     *         end of the current text).
     */
    private boolean isCursorAtEndOfTypedText() {
        final int selectionStart = mDelegate.getSelectionStart();
        final int selectionEnd = mDelegate.getSelectionEnd();

        int expectedSelectionStart = mDelegate.getText().getSpanStart(mAutocompleteSpan);
        int expectedSelectionEnd = mDelegate.getText().length();
        if (expectedSelectionStart < 0) {
            expectedSelectionStart = expectedSelectionEnd;
        }

        return selectionStart == expectedSelectionStart && selectionEnd == expectedSelectionEnd;
    }

    @Override
    public String getTextWithoutAutocomplete() {
        int autoCompleteIndex = mDelegate.getText().getSpanStart(mAutocompleteSpan);
        if (autoCompleteIndex < 0) {
            return getTextWithAutocomplete();
        } else {
            return getTextWithAutocomplete().substring(0, autoCompleteIndex);
        }
    }

    @Override
    public boolean hasAutocomplete() {
        return mDelegate.getText().getSpanStart(mAutocompleteSpan) >= 0
                || mAutocompleteSpan.mAutocompleteText != null
                || mAutocompleteSpan.mUserText != null;
    }

    @Override
    public boolean shouldAutocomplete() {
        if (mLastEditWasDelete) return false;
        Editable text = mDelegate.getText();
        return isCursorAtEndOfTypedText() && !mLastEditWasPaste && mBatchEditNestCount == 0
                && BaseInputConnection.getComposingSpanEnd(text)
                == BaseInputConnection.getComposingSpanStart(text);
    }

    private void onPostEndBatchEdit() {
        if (mSelectionChangedInBatchMode) {
            validateSelection(mDelegate.getSelectionStart(), mDelegate.getSelectionEnd());
            mSelectionChangedInBatchMode = false;
        }

        String newText = mDelegate.getText().toString();
        if (!TextUtils.equals(mBeforeBatchEditFullText, newText)
                || mDelegate.getText().getSpanStart(mAutocompleteSpan)
                        != mBeforeBatchEditAutocompleteIndex) {
            // If the text being typed is a single character that matches the next character in the
            // previously visible autocomplete text, we reapply the autocomplete text to prevent
            // a visual flickering when the autocomplete text is cleared and then quickly reapplied
            // when the next round of suggestions is received.
            if (shouldAutocomplete() && mBeforeBatchEditAutocompleteIndex != -1
                    && mBeforeBatchEditFullText != null
                    && mBeforeBatchEditFullText.startsWith(newText) && !mTextDeletedInBatchMode
                    && newText.length() - mBeforeBatchEditAutocompleteIndex == 1) {
                setAutocompleteText(newText, mBeforeBatchEditFullText.substring(newText.length()));
            }
            notifyAutocompleteTextStateChanged(mTextDeletedInBatchMode, true);
        }

        mTextDeletedInBatchMode = false;
        mBeforeBatchEditAutocompleteIndex = -1;
        mBeforeBatchEditFullText = null;
        updateSelectionForTesting();
    }

    @Override
    public void onSelectionChanged(int selStart, int selEnd) {
        if (DEBUG) Log.i(TAG, "onSelectionChanged -- selStart: %d, selEnd: %d", selStart, selEnd);
        if (mBatchEditNestCount == 0) {
            int beforeTextLength = mDelegate.getText().length();
            if (validateSelection(selStart, selEnd)) {
                boolean textDeleted = mDelegate.getText().length() < beforeTextLength;
                notifyAutocompleteTextStateChanged(textDeleted, false);
            }
            updateSelectionForTesting();
        } else {
            mSelectionChangedInBatchMode = true;
        }
    }

    /**
     * Validates the selection and clears the autocomplete span if needed.  The autocomplete text
     * will be deleted if the selection occurs entirely before the autocomplete region.
     *
     * @param selStart The start of the selection.
     * @param selEnd The end of the selection.
     * @return Whether the autocomplete span was removed as a result of this validation.
     */
    private boolean validateSelection(int selStart, int selEnd) {
        Editable text = mDelegate.getText();
        int spanStart = text.getSpanStart(mAutocompleteSpan);
        int spanEnd = text.getSpanEnd(mAutocompleteSpan);

        if (DEBUG) {
            Log.i(TAG, "validateSelection -- selStart: %d, selEnd: %d, spanStart: %d, spanEnd: %d",
                    selStart, selEnd, spanStart, spanEnd);
        }

        if (spanStart >= 0 && (spanStart != selStart || spanEnd != selEnd)) {
            CharSequence previousAutocompleteText = mAutocompleteSpan.mAutocompleteText;

            // On selection changes, the autocomplete text has been accepted by the user or needs
            // to be deleted below.
            mAutocompleteSpan.clearSpan();

            // The autocomplete text will be deleted any time the selection occurs entirely before
            // the start of the autocomplete text.  This is required because certain keyboards will
            // insert characters temporarily when starting a key entry gesture (whether it be
            // swyping a word or long pressing to get a special character).  When this temporary
            // character appears, Chrome may decide to append some autocomplete, but the keyboard
            // will then remove this temporary character only while leaving the autocomplete text
            // alone.  See crbug/273763 for more details.
            if (selEnd <= spanStart
                    && TextUtils.equals(previousAutocompleteText,
                               text.subSequence(spanStart, text.length()))) {
                text.delete(spanStart, text.length());
            }
            return true;
        }
        return false;
    }

    @Override
    public void onFocusChanged(boolean focused) {
        if (!focused) mAutocompleteSpan.clearSpan();
    }

    @Override
    public void setAutocompleteText(CharSequence userText, CharSequence inlineAutocompleteText) {
        if (DEBUG) {
            Log.i(TAG, "setAutocompleteText -- userText: %s, inlineAutocompleteText: %s", userText,
                    inlineAutocompleteText);
        }

        int autocompleteIndex = userText.length();
        String previousText = getTextWithAutocomplete();
        CharSequence newText = TextUtils.concat(userText, inlineAutocompleteText);

        setIgnoreTextChangeFromAutocomplete(true);

        if (!TextUtils.equals(previousText, newText)) {
            // The previous text may also have included autocomplete text, so we only
            // append the new autocomplete text that has changed.
            if (TextUtils.indexOf(newText, previousText) == 0) {
                mDelegate.append(newText.subSequence(previousText.length(), newText.length()));
            } else {
                mDelegate.replaceAllTextFromAutocomplete(newText.toString());
            }
        }

        if (mDelegate.getSelectionStart() != autocompleteIndex
                || mDelegate.getSelectionEnd() != mDelegate.getText().length()) {
            mDelegate.setSelection(autocompleteIndex, mDelegate.getText().length());

            if (inlineAutocompleteText.length() != 0) {
                // Sending a TYPE_VIEW_TEXT_SELECTION_CHANGED accessibility event causes the
                // previous TYPE_VIEW_TEXT_CHANGED event to be swallowed. As a result the user
                // hears the autocomplete text but *not* the text they typed. Instead we send a
                // TYPE_ANNOUNCEMENT event, which doesn't swallow the text-changed event.
                mDelegate.announceForAccessibility(inlineAutocompleteText);
            }
        }

        boolean emptyAutocomplete = TextUtils.isEmpty(inlineAutocompleteText);
        if (emptyAutocomplete) {
            mAutocompleteSpan.clearSpan();
        } else {
            mAutocompleteSpan.setSpan(userText, inlineAutocompleteText);
        }

        setIgnoreTextChangeFromAutocomplete(false);
        if (DEBUG) Log.i(TAG, "setAutocompleteText finished");
    }

    @Override
    public String getAutocompleteText() {
        if (!hasAutocomplete()) return "";
        return mAutocompleteSpan.mAutocompleteText.toString();
    }

    @Override
    public void onTextChanged(CharSequence text, int start, int lengthBefore, int lengthAfter) {
        if (DEBUG) {
            Log.i(TAG, "onTextChanged: [%s], start: %d, bef: %d, aft: %d", text, start,
                    lengthBefore, lengthAfter);
        }
        boolean textDeleted = lengthAfter == 0;
        if (mBatchEditNestCount == 0) {
            notifyAutocompleteTextStateChanged(textDeleted, true);
        } else {
            // crbug.com/764749
            Log.w(TAG, "onTextChanged: in batch edit");
            mTextDeletedInBatchMode = textDeleted;
        }
        mLastEditWasPaste = false;
    }

    @Override
    public void onSetText(CharSequence text) {
        if (DEBUG) Log.i(TAG, "onSetText: [%s]", text);
        // Verify the autocomplete is still valid after the text change.
        if (mAutocompleteSpan.mUserText != null && mAutocompleteSpan.mAutocompleteText != null) {
            if (mDelegate.getText().getSpanStart(mAutocompleteSpan) < 0) {
                mAutocompleteSpan.clearSpan();
            } else {
                clearAutocompleteSpanIfInvalid();
            }
        }
    }

    private void clearAutocompleteSpanIfInvalid() {
        Editable editableText = mDelegate.getEditableText();
        CharSequence previousUserText = mAutocompleteSpan.mUserText;
        CharSequence previousAutocompleteText = mAutocompleteSpan.mAutocompleteText;
        if (editableText.length()
                != (previousUserText.length() + previousAutocompleteText.length())) {
            mAutocompleteSpan.clearSpan();
        } else if (TextUtils.indexOf(mDelegate.getText(), previousUserText) != 0
                || TextUtils.indexOf(
                           mDelegate.getText(), previousAutocompleteText, previousUserText.length())
                        != 0) {
            mAutocompleteSpan.clearSpan();
        }
    }

    @Override
    public InputConnection getInputConnection() {
        return mInputConnection;
    }

    private final InputConnectionWrapper mInputConnection = new InputConnectionWrapper(null, true) {
        private final char[] mTempSelectionChar = new char[1];

        @Override
        public boolean beginBatchEdit() {
            ++mBatchEditNestCount;
            if (mBatchEditNestCount == 1) {
                if (DEBUG) Log.i(TAG, "beginBatchEdit");
                mBeforeBatchEditAutocompleteIndex =
                        mDelegate.getText().getSpanStart(mAutocompleteSpan);
                mBeforeBatchEditFullText = mDelegate.getText().toString();
                boolean retVal = super.beginBatchEdit();
                mTextDeletedInBatchMode = false;
                return retVal;
            }
            return super.beginBatchEdit();
        }

        @Override
        public boolean endBatchEdit() {
            mBatchEditNestCount = Math.max(mBatchEditNestCount - 1, 0);
            if (mBatchEditNestCount == 0) {
                if (DEBUG) Log.i(TAG, "endBatchEdit");
                boolean retVal = super.endBatchEdit();
                onPostEndBatchEdit();
                return retVal;
            }
            return super.endBatchEdit();
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            if (DEBUG) Log.i(TAG, "commitText: [%s]", text);
            Editable currentText = mDelegate.getText();
            if (currentText == null) return super.commitText(text, newCursorPosition);

            int selectionStart = Selection.getSelectionStart(currentText);
            int selectionEnd = Selection.getSelectionEnd(currentText);
            int autocompleteIndex = currentText.getSpanStart(mAutocompleteSpan);
            // If the text being committed is a single character that matches the next character
            // in the selection (assumed to be the autocomplete text), we only move the text
            // selection instead clearing the autocomplete text causing flickering as the
            // autocomplete text will appear once the next suggestions are received.
            //
            // To be confident that the selection is an autocomplete, we ensure the selection
            // is at least one character and the end of the selection is the end of the
            // currently entered text.
            if (newCursorPosition == 1 && selectionStart > 0 && selectionStart != selectionEnd
                    && selectionEnd >= currentText.length() && autocompleteIndex == selectionStart
                    && text.length() == 1) {
                currentText.getChars(selectionStart, selectionStart + 1, mTempSelectionChar, 0);
                if (mTempSelectionChar[0] == text.charAt(0)) {
                    if (mDelegate.isAccessibilityEnabled()) {
                        // Since the text isn't changing, TalkBack won't read out the typed
                        // characters. To work around this, explicitly send an accessibility event.
                        // crbug.com/416595
                        AccessibilityEvent event = AccessibilityEvent.obtain(
                                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
                        event.setBeforeText(currentText.toString().substring(0, selectionStart));
                        event.setFromIndex(selectionStart);
                        event.setRemovedCount(0);
                        event.setAddedCount(1);
                        mDelegate.sendAccessibilityEventUnchecked(event);
                    }
                    setAutocompleteText(currentText.subSequence(0, selectionStart + 1),
                            currentText.subSequence(selectionStart + 1, selectionEnd));
                    if (mBatchEditNestCount == 0) {
                        notifyAutocompleteTextStateChanged(false, false);
                    }
                    return true;
                }
            }

            boolean retVal = super.commitText(text, newCursorPosition);

            // Ensure the autocomplete span is removed if it is no longer valid after committing
            // the text.
            if (currentText.getSpanStart(mAutocompleteSpan) >= 0) {
                clearAutocompleteSpanIfInvalid();
            }

            return retVal;
        }

        @Override
        public boolean setComposingText(CharSequence text, int newCursorPosition) {
            if (DEBUG) Log.i(TAG, "setComposingText: [%s]", text);
            Editable currentText = mDelegate.getText();
            int autoCompleteSpanStart = currentText.getSpanStart(mAutocompleteSpan);
            if (autoCompleteSpanStart >= 0) {
                int composingEnd = BaseInputConnection.getComposingSpanEnd(currentText);

                // On certain device/keyboard combinations, the composing regions are specified
                // with a noticeable delay after the initial character is typed, and in certain
                // circumstances it does not check that the current state of the text matches
                // the expectations of it's composing region. For example, you can be typing:
                //   chrome://f
                // Chrome will autocomplete to:
                //   chrome://f[lags]
                // And after the autocomplete has been set, the keyboard will set the composing
                // region to the last character and it assumes it is 'f' as it was the last
                // character the keyboard sent.  If we commit this composition, the text will
                // look like:
                //   chrome://flag[f]
                // And if we use the autocomplete clearing logic below, it will look like:
                //   chrome://f[f]
                // To work around this, we see if the composition matches all the characters
                // prior to the autocomplete and just readjust the composing region to be that
                // subset.
                //
                // See crbug.com/366732
                if (composingEnd == currentText.length()
                        && autoCompleteSpanStart >= text.length()) {
                    CharSequence trailingText = currentText.subSequence(
                            autoCompleteSpanStart - text.length(), autoCompleteSpanStart);
                    if (TextUtils.equals(trailingText, text)) {
                        setComposingRegion(
                                autoCompleteSpanStart - text.length(), autoCompleteSpanStart);
                    }
                }

                // Once composing text is being modified, the autocomplete text has been
                // accepted or has to be deleted.
                mAutocompleteSpan.clearSpan();
                Selection.setSelection(currentText, autoCompleteSpanStart);
                currentText.delete(autoCompleteSpanStart, currentText.length());
            }
            return super.setComposingText(text, newCursorPosition);
        }
    };

    @Override
    public InputConnection onCreateInputConnection(InputConnection superInputConnection) {
        if (DEBUG) Log.i(TAG, "onCreateInputConnection");
        mLastUpdateSelStart = mDelegate.getSelectionStart();
        mLastUpdateSelEnd = mDelegate.getSelectionEnd();
        mBatchEditNestCount = 0;
        mInputConnection.setTarget(superInputConnection);
        return mInputConnection;
    }

    private void notifyAutocompleteTextStateChanged(boolean textDeleted, boolean updateDisplay) {
        if (DEBUG) {
            Log.i(TAG, "notifyAutocompleteTextStateChanged: DEL[%b] DIS[%b] IGN[%b]", textDeleted,
                    updateDisplay, mIgnoreTextChangeFromAutocomplete);
        }
        if (mIgnoreTextChangeFromAutocomplete) {
            // crbug.com/764749
            Log.w(TAG, "notification ignored");
            return;
        }
        mLastEditWasDelete = textDeleted;
        mDelegate.onAutocompleteTextStateChanged(updateDisplay);
        // Occasionally, was seeing the selection in the URL not being cleared during
        // very rapid editing.  This is here to hopefully force a selection reset during
        // deletes.
        if (textDeleted) {
            mDelegate.setSelection(mDelegate.getSelectionStart(), mDelegate.getSelectionStart());
        }
    }

    @Override
    public void onPaste() {
        if (DEBUG) Log.i(TAG, "onPaste");
        mLastEditWasPaste = true;
    }

    /**
     * Simple span used for tracking the current autocomplete state.
     */
    private class AutocompleteSpan {
        private CharSequence mUserText;
        private CharSequence mAutocompleteText;

        /**
         * Adds the span to the current text.
         * @param userText The user entered text.
         * @param autocompleteText The autocomplete text being appended.
         */
        public void setSpan(CharSequence userText, CharSequence autocompleteText) {
            Editable text = mDelegate.getText();
            text.removeSpan(this);
            mAutocompleteText = autocompleteText;
            mUserText = userText;
            text.setSpan(this, userText.length(), text.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        /** Removes this span from the current text and clears the internal state. */
        public void clearSpan() {
            mDelegate.getText().removeSpan(this);
            mAutocompleteText = null;
            mUserText = null;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mDelegate.super_dispatchKeyEvent(event);
    }

    private void updateSelectionForTesting() {
        int selStart = mDelegate.getSelectionStart();
        int selEnd = mDelegate.getSelectionEnd();
        if (selStart == mLastUpdateSelStart && selEnd == mLastUpdateSelEnd) return;

        mLastUpdateSelStart = selStart;
        mLastUpdateSelEnd = selEnd;
        mDelegate.onUpdateSelectionForTesting(selStart, selEnd);
    }

    @Override
    public boolean shouldIgnoreAccessibilityEvent() {
        return false;
    }
}
