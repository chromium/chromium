// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.os.Bundle;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputContentInfo;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

class AutocompleteInputConnection extends InputConnectionWrapper {
    private static final String TAG = "AutocompleteInput";
    private static final boolean DEBUG = false;

    /** Interface defining the delegate for handling input-related actions. */
    public interface InputDelegate {
        /**
         * @return Whether the input are in the batch edit.
         */
        boolean isInBatchEdit();

        /**
         * @return Whether the input are in the first batch edit.
         */
        boolean isInFirstBatchEdit();

        /** Increments the count of batch edits. */
        void incrementBatchEditCount();

        /** Decrements the count of batch edits. */
        void decrementBatchEditCount();

        /**
         * Gets the current state of autocomplete.
         *
         * @return The current autocomplete state.
         */
        AutocompleteState getCurrentState();

        /**
         * Gets the previously set state of autocomplete.
         *
         * @return The previously set autocomplete state.
         */
        AutocompleteState getPreviouslySetState();

        /**
         * Gets the span cursor controller.
         *
         * @return The span cursor controller.
         */
        SpanCursorController getSpanCursorController();

        /**
         * Sets whether the last edit was typing.
         *
         * @param wasTyping True if the last edit was typing, false otherwise.
         */
        void setLastEditWasTyping(boolean wasTyping);

        /**
         * Gets the {@link AutocompleteEditTextModelBase.Delegate}.
         *
         * @return The delegate.
         */
        AutocompleteEditTextModelBase.Delegate getAutocompleteEditTextModelBaseDelegate();

        /**
         * Gets the postfix delete command to execute on the next IME begin command.
         *
         * @return The postfix delete command.
         */
        int getDeletePostfixOnNextBeginImeCommand();

        /**
         * Sets the postfix delete command to execute on the next IME begin command.
         *
         * @param postfix The postfix delete command.
         */
        void setDeletePostfixOnNextBeginImeCommand(int postfix);

        /**
         * @return Whether any autocomplete information is specified on the current text.
         */
        @VisibleForTesting
        boolean hasAutocomplete();

        /** Clear autocomplete text in the current text. */
        void clearAutocompleteText();

        /** Notifies that the autocomplete text state has changed. */
        void notifyAutocompleteTextStateChanged();

        /** Updates the selection, primarily for testing purposes. */
        void updateSelectionForTesting();

        /**
         * Determines if the composition should be finished upon deletion.
         *
         * @return True if the composition should be finished on deletion, false otherwise.
         */
        boolean shouldFinishCompositionOnDeletion();
    }

    private final AutocompleteState mPreBatchEditState;
    private final InputDelegate mInputDelegate;

    public AutocompleteInputConnection(InputDelegate inputDelegate) {
        super(null, true);
        mInputDelegate = inputDelegate;
        mPreBatchEditState = new AutocompleteState(mInputDelegate.getCurrentState());
    }

    private boolean incrementBatchEditCount() {
        mInputDelegate.incrementBatchEditCount();
        // After the outermost super.beginBatchEdit(), EditText will stop selection change
        // update to the IME app.
        return super.beginBatchEdit();
    }

    private boolean decrementBatchEditCount() {
        mInputDelegate.decrementBatchEditCount();
        boolean retVal = super.endBatchEdit();
        if (!mInputDelegate.isInBatchEdit()) {
            // At the outermost super.endBatchEdit(), EditText will resume selection change
            // update to the IME app.
            mInputDelegate.updateSelectionForTesting();
        }
        return retVal;
    }

    public void commitAutocomplete() {
        if (DEBUG) Log.i(TAG, "commitAutocomplete");
        if (!mInputDelegate.hasAutocomplete()) return;

        String autocompleteText = mInputDelegate.getCurrentState().getAutocompleteText().get();

        mInputDelegate.getCurrentState().commitAutocompleteText();
        // Invalidate previous state.
        mInputDelegate.getPreviouslySetState().copyFrom(mInputDelegate.getCurrentState());
        mInputDelegate.setLastEditWasTyping(false);

        if (!mInputDelegate.isInBatchEdit()) {
            incrementBatchEditCount(); // avoids additional notifyAutocompleteTextStateChanged()
            mInputDelegate.getSpanCursorController().commitSpan();
            decrementBatchEditCount();
        } else {
            // We have already removed span in the onBeginImeCommand(), just append the text.
            mInputDelegate.getAutocompleteEditTextModelBaseDelegate().append(autocompleteText);
        }
    }

    @Override
    public boolean beginBatchEdit() {
        if (DEBUG) Log.i(TAG, "beginBatchEdit");
        onBeginImeCommand();
        incrementBatchEditCount();
        return onEndImeCommand();
    }

    /**
     * Always call this at the beginning of any IME command. Compare this with beginBatchEdit()
     * which is by itself an IME command.
     *
     * @return {@code true} if the batch edit is still in progress. {@code false} otherwise.
     */
    public boolean onBeginImeCommand() {
        if (DEBUG) Log.i(TAG, "onBeginImeCommand: " + mInputDelegate.isInBatchEdit());
        boolean retVal = incrementBatchEditCount();
        if (mInputDelegate.isInFirstBatchEdit()) {
            mPreBatchEditState.copyFrom(mInputDelegate.getCurrentState());
        } else if (mInputDelegate.getDeletePostfixOnNextBeginImeCommand() > 0) {
            // Note: in languages that rely on character composition, the last incomplete
            // character may not be recognized as part of the string, but it may still be
            // accounted for by the mDeletePostfixOnNextBeginImeCommand.
            // In such case, the text below is actually shorter than the user input, and the
            // computed string boundaries enter negative index space.
            int len = mInputDelegate.getAutocompleteEditTextModelBaseDelegate().getText().length();
            if (mInputDelegate.getDeletePostfixOnNextBeginImeCommand() > len) {
                mInputDelegate.setDeletePostfixOnNextBeginImeCommand(len);
            }
            mInputDelegate
                    .getAutocompleteEditTextModelBaseDelegate()
                    .getText()
                    .delete(len - mInputDelegate.getDeletePostfixOnNextBeginImeCommand(), len);
        }
        mInputDelegate.setDeletePostfixOnNextBeginImeCommand(0);
        mInputDelegate.getSpanCursorController().removeAutocompleteSpan();
        return retVal;
    }

    private void restoreBackspacedText(String diff) {
        if (DEBUG) Log.i(TAG, "restoreBackspacedText. diff: " + diff);

        if (mInputDelegate.isInBatchEdit()) {
            // If batch edit hasn't finished, we will restore backspaced text only for visual
            // effects. However, for internal operations to work correctly, we need to remove
            // the restored diff at the beginning of next IME operation.
            mInputDelegate.setDeletePostfixOnNextBeginImeCommand(diff.length());
        }
        if (!mInputDelegate.isInBatchEdit()) { // only at the outermost batch edit
            if (mInputDelegate.shouldFinishCompositionOnDeletion()) super.finishComposingText();
        }
        incrementBatchEditCount(); // avoids additional notifyAutocompleteTextStateChanged()
        Editable editable =
                mInputDelegate.getAutocompleteEditTextModelBaseDelegate().getEditableText();
        editable.append(diff);
        decrementBatchEditCount();
    }

    private boolean setAutocompleteSpan() {
        mInputDelegate.getSpanCursorController().removeAutocompleteSpan();
        if (DEBUG) {
            Log.i(
                    TAG,
                    "setAutocompleteSpan. %s->%s",
                    mInputDelegate.getPreviouslySetState(),
                    mInputDelegate.getCurrentState());
        }
        if (!mInputDelegate.getCurrentState().isCursorAtEndOfUserText()) return false;

        if (mInputDelegate
                .getCurrentState()
                .reuseAutocompleteTextIfPrefixExtension(mInputDelegate.getPreviouslySetState())) {
            mInputDelegate.getSpanCursorController().setSpan(mInputDelegate.getCurrentState());
            return true;
        } else {
            return false;
        }
    }

    @Override
    public boolean endBatchEdit() {
        if (DEBUG) Log.i(TAG, "endBatchEdit");
        onBeginImeCommand();
        decrementBatchEditCount();
        return onEndImeCommand();
    }

    /**
     * Always call this at the end of an IME command. Compare this with endBatchEdit() which is by
     * itself an IME command.
     *
     * @return {@code true} if the batch edit is still in progress. {@code false} otherwise.
     */
    public boolean onEndImeCommand() {
        if (DEBUG) Log.i(TAG, "onEndImeCommand: " + mInputDelegate.isInBatchEdit());
        AutocompleteState currentState = mInputDelegate.getCurrentState();
        String diff = currentState.getBackwardDeletedTextFrom(mPreBatchEditState);
        if (diff != null) {
            // Update selection first such that keyboard app gets what it expects.
            boolean retVal = decrementBatchEditCount();

            if (mPreBatchEditState.getAutocompleteText().isPresent()) {
                // Undo delete to retain the last character and only remove autocomplete text.
                restoreBackspacedText(diff);
            }
            mInputDelegate.setLastEditWasTyping(false);
            mInputDelegate.clearAutocompleteText();
            mInputDelegate.notifyAutocompleteTextStateChanged();
            return retVal;
        }
        if (!setAutocompleteSpan()) {
            mInputDelegate.clearAutocompleteText();
        }
        boolean retVal = decrementBatchEditCount();
        // Simply typed some characters or whole text selection has been overridden.
        if (currentState.isForwardTypedFrom(mPreBatchEditState)
                || (mPreBatchEditState.isWholeUserTextSelected()
                        && currentState.getUserText().length() > 0
                        && currentState.isCursorAtEndOfUserText())) {
            mInputDelegate.setLastEditWasTyping(true);
        }
        mInputDelegate.notifyAutocompleteTextStateChanged();
        return retVal;
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        if (DEBUG) Log.i(TAG, "commitText: " + text);
        onBeginImeCommand();
        boolean retVal = super.commitText(text, newCursorPosition);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        if (DEBUG) Log.i(TAG, "setComposingText: " + text);
        onBeginImeCommand();
        boolean retVal = super.setComposingText(text, newCursorPosition);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        if (DEBUG) Log.i(TAG, "setComposingRegion: [%d,%d]", start, end);
        onBeginImeCommand();
        boolean retVal = super.setComposingRegion(start, end);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean finishComposingText() {
        if (DEBUG) Log.i(TAG, "finishComposingText");
        onBeginImeCommand();
        boolean retVal = super.finishComposingText();
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean deleteSurroundingText(final int beforeLength, final int afterLength) {
        if (DEBUG) Log.i(TAG, "deleteSurroundingText [%d,%d]", beforeLength, afterLength);
        onBeginImeCommand();
        boolean retVal = super.deleteSurroundingText(beforeLength, afterLength);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean setSelection(final int start, final int end) {
        if (DEBUG) Log.i(TAG, "setSelection [%d,%d]", start, end);
        onBeginImeCommand();
        boolean retVal = super.setSelection(start, end);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean performEditorAction(final int editorAction) {
        if (DEBUG) Log.i(TAG, "performEditorAction: " + editorAction);
        onBeginImeCommand();
        commitAutocomplete();
        boolean retVal = super.performEditorAction(editorAction);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean sendKeyEvent(final KeyEvent event) {
        if (DEBUG) Log.i(TAG, "sendKeyEvent: " + event.getKeyCode());
        onBeginImeCommand();
        boolean retVal = super.sendKeyEvent(event);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public ExtractedText getExtractedText(final ExtractedTextRequest request, final int flags) {
        if (DEBUG) Log.i(TAG, "getExtractedText");
        onBeginImeCommand();
        ExtractedText retVal = super.getExtractedText(request, flags);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public CharSequence getTextAfterCursor(final int n, final int flags) {
        if (DEBUG) Log.i(TAG, "getTextAfterCursor");
        onBeginImeCommand();
        CharSequence retVal = super.getTextAfterCursor(n, flags);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public CharSequence getTextBeforeCursor(final int n, final int flags) {
        if (DEBUG) Log.i(TAG, "getTextBeforeCursor");
        onBeginImeCommand();
        CharSequence retVal = super.getTextBeforeCursor(n, flags);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public CharSequence getSelectedText(final int flags) {
        if (DEBUG) Log.i(TAG, "getSelectedText");
        onBeginImeCommand();
        CharSequence retVal = super.getSelectedText(flags);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        if (DEBUG) Log.i(TAG, "commitCompletion");
        onBeginImeCommand();
        boolean retVal = super.commitCompletion(text);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean commitContent(InputContentInfo inputContentInfo, int flags, Bundle opts) {
        if (DEBUG) Log.i(TAG, "commitContent");
        onBeginImeCommand();
        boolean retVal = super.commitContent(inputContentInfo, flags, opts);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean commitCorrection(CorrectionInfo correctionInfo) {
        if (DEBUG) Log.i(TAG, "commitCorrection");
        onBeginImeCommand();
        boolean retVal = super.commitCorrection(correctionInfo);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        if (DEBUG) Log.i(TAG, "deleteSurroundingTextInCodePoints");
        onBeginImeCommand();
        boolean retVal = super.deleteSurroundingTextInCodePoints(beforeLength, afterLength);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public int getCursorCapsMode(int reqModes) {
        if (DEBUG) Log.i(TAG, "getCursorCapsMode");
        onBeginImeCommand();
        int retVal = super.getCursorCapsMode(reqModes);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean requestCursorUpdates(int cursorUpdateMode) {
        if (DEBUG) Log.i(TAG, "requestCursorUpdates");
        onBeginImeCommand();
        boolean retVal = super.requestCursorUpdates(cursorUpdateMode);
        onEndImeCommand();
        return retVal;
    }

    @Override
    public boolean clearMetaKeyStates(int states) {
        if (DEBUG) Log.i(TAG, "clearMetaKeyStates");
        onBeginImeCommand();
        boolean retVal = super.clearMetaKeyStates(states);
        onEndImeCommand();
        return retVal;
    }
}
