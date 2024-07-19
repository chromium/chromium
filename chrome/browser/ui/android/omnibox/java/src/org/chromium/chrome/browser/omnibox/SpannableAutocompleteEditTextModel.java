// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.view.inputmethod.InputContentInfo;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.Optional;
import java.util.regex.Pattern;

/**
 * An autocomplete model that appends autocomplete text at the end of query/URL text as
 * SpannableString. By wrapping all the keyboard related operations in a batch edit, we can
 * effectively hide the existence of autocomplete text from keyboard.
 */
public class SpannableAutocompleteEditTextModel implements AutocompleteEditTextModelBase {
    private static final String TAG = "SpanAutocomplete";
    private static final boolean DEBUG = false;

    // A pattern that matches strings consisting of English and European character sets, numbers,
    // punctuations, and a white space.
    private static final Pattern NON_COMPOSITIONAL_TEXT_PATTERN =
            Pattern.compile(
                    "[\\p{script=latin}\\p{script=cyrillic}\\p{script=greek}\\p{script=hebrew}\\p{Punct}"
                        + " 0-9]*");

    private final AutocompleteEditTextModelBase.Delegate mDelegate;

    // The current state that reflects EditText view's current state through callbacks such as
    // onSelectionChanged() and onTextChanged(). It reflects all the latest changes even in a batch
    // edit. The autocomplete text here is meaningless in the middle of a change.
    private final AutocompleteState mCurrentState;

    // This keeps track of the state in which previous notification was sent. It prevents redundant
    // or unnecessary notification.
    private final AutocompleteState mPreviouslyNotifiedState;

    // This keeps track of the autocompletetext that we need to show (at the end of batch edit if
    // we are in the middle of it), and also the matching user text that it should be appended to.
    // Note that this potentially allows the controller to update in a delayed manner.
    private final AutocompleteState mPreviouslySetState;

    private final SpanCursorController mSpanCursorController;

    private AutocompleteInputConnection mInputConnection;
    private boolean mLastEditWasTyping = true;
    private boolean mIgnoreTextChangeFromAutocomplete = true;
    private int mBatchEditNestCount;
    private int mDeletePostfixOnNextBeginImeCommand;

    // For testing.
    private int mLastUpdateSelStart;
    private int mLastUpdateSelEnd;

    // This controls whether AutocompleteEditText is permitted to pass-through specific
    // Accessibility announcements, in particular the TEXT_CHANGED and TEXT_SELECTION_CHANGED.
    // The only events of the above type that are allowed are ones coming from
    // SpannableAutocompleteEditTextModel.
    private boolean mDelegateShouldIgnoreAccessibilityEvents = true;

    public SpannableAutocompleteEditTextModel(
            AutocompleteEditTextModelBase.Delegate delegate, Context context) {
        if (DEBUG) Log.i(TAG, "constructor");
        mDelegate = delegate;
        mCurrentState =
                new AutocompleteState(
                        delegate.getText().toString(),
                        null,
                        null,
                        delegate.getSelectionStart(),
                        delegate.getSelectionEnd());
        mPreviouslyNotifiedState = new AutocompleteState(mCurrentState);
        mPreviouslySetState = new AutocompleteState(mCurrentState);

        mSpanCursorController = new SpanCursorController(delegate, context);
    }

    @Override
    public InputConnection onCreateInputConnection(InputConnection inputConnection) {
        mLastUpdateSelStart = mDelegate.getSelectionStart();
        mLastUpdateSelEnd = mDelegate.getSelectionEnd();
        mBatchEditNestCount = 0;
        if (inputConnection == null) {
            if (DEBUG) Log.i(TAG, "onCreateInputConnection: null");
            mInputConnection = null;
            return null;
        }
        if (DEBUG) Log.i(TAG, "onCreateInputConnection");
        mInputConnection = new AutocompleteInputConnection();
        mInputConnection.setTarget(inputConnection);
        return mInputConnection;
    }

    public void setInputConnectionForTesting(AutocompleteInputConnection connection) {
        mInputConnection = connection;
    }

    private void sendAccessibilityEventForUserTextChange(
            AutocompleteState oldState, AutocompleteState newState) {
        int addedCount = -1;
        int removedCount = -1;
        int fromIndex = -1;

        if (newState.isBackwardDeletedFrom(oldState)) {
            addedCount = 0;
            removedCount = oldState.getText().length() - newState.getUserText().length();
            fromIndex = newState.getUserText().length();
        } else if (newState.isForwardTypedFrom(oldState)) {
            addedCount = newState.getUserText().length() - oldState.getUserText().length();
            removedCount = oldState.getAutocompleteText().map(t -> t.length()).orElse(0);
            fromIndex = oldState.getUserText().length();
        } else if (newState.getUserText().equals(oldState.getUserText())) {
            addedCount = 0;
            removedCount = oldState.getAutocompleteText().map(t -> t.length()).orElse(0);
            fromIndex = oldState.getUserText().length();
        } else {
            // Assume that the whole text has been replaced.
            addedCount = newState.getText().length();
            removedCount = oldState.getUserText().length();
            fromIndex = 0;
        }

        mDelegateShouldIgnoreAccessibilityEvents = false;
        if (!oldState.getText().equals(newState.getText())
                && (addedCount != 0 || removedCount != 0)) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
            event.setBeforeText(oldState.getText());
            event.setFromIndex(fromIndex);
            event.setRemovedCount(removedCount);
            event.setAddedCount(addedCount);
            mDelegate.sendAccessibilityEventUnchecked(event);
        }

        if (oldState.getSelStart() != newState.getSelStart()
                || oldState.getSelEnd() != newState.getSelEnd()) {
            mDelegate.sendAccessibilityEventUnchecked(
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED));
        }
        mDelegateShouldIgnoreAccessibilityEvents = true;
    }

    private void sendAccessibilityEventForAppendingAutocomplete(AutocompleteState newState) {
        if (!newState.getAutocompleteText().isPresent()) return;
        // Note that only text changes and selection does not change.
        AccessibilityEvent eventTextChanged =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
        eventTextChanged.setBeforeText(newState.getUserText());
        eventTextChanged.setFromIndex(newState.getUserText().length());
        eventTextChanged.setRemovedCount(0);
        eventTextChanged.setAddedCount(
                newState.getAutocompleteText().map(t -> t.length()).orElse(0));
        mDelegateShouldIgnoreAccessibilityEvents = false;
        mDelegate.sendAccessibilityEventUnchecked(eventTextChanged);
        mDelegateShouldIgnoreAccessibilityEvents = true;
    }

    private void notifyAccessibilityService() {
        if (mCurrentState.equals(mPreviouslyNotifiedState)) return;
        if (!AccessibilityState.isAccessibilityEnabled()) return;
        sendAccessibilityEventForUserTextChange(mPreviouslyNotifiedState, mCurrentState);
        // Read autocomplete text separately.
        sendAccessibilityEventForAppendingAutocomplete(mCurrentState);
    }

    private void notifyAutocompleteTextStateChanged() {
        if (DEBUG) {
            Log.i(
                    TAG,
                    "notifyAutocompleteTextStateChanged PRV[%s] CUR[%s] IGN[%b]",
                    mPreviouslyNotifiedState,
                    mCurrentState,
                    mIgnoreTextChangeFromAutocomplete);
        }
        if (mBatchEditNestCount > 0) {
            if (DEBUG) Log.i(TAG, "Did not notify - in batch edit.");
            return;
        }
        if (mCurrentState.equals(mPreviouslyNotifiedState)) {
            if (DEBUG) Log.i(TAG, "Did not notify - no change.");
            return;
        }
        notifyAccessibilityService();
        if (mCurrentState.getUserText().equals(mPreviouslyNotifiedState.getUserText())
                && (mCurrentState.getAutocompleteText().isPresent()
                        || !mPreviouslyNotifiedState.getAutocompleteText().isPresent())) {
            // Nothing has changed except that autocomplete text has been set or modified. Or
            // selection change did not affect autocomplete text. Autocomplete text is set by the
            // controller, so only text change or deletion of autocomplete text should be notified.
            mPreviouslyNotifiedState.copyFrom(mCurrentState);
            return;
        }
        mPreviouslyNotifiedState.copyFrom(mCurrentState);
        if (mIgnoreTextChangeFromAutocomplete) {
            if (DEBUG) Log.i(TAG, "Did not notify - ignored.");
            return;
        }
        // The current model's mechanism always moves the cursor at the end of user text, so we
        // don't need to update the display.
        mDelegate.onAutocompleteTextStateChanged(/* updateDisplay= */ false);
    }

    private void clearAutocompleteText() {
        if (DEBUG) Log.i(TAG, "clearAutocomplete");
        mPreviouslySetState.clearAutocompleteText();
        mCurrentState.clearAutocompleteText();
    }

    private void clearAutocompleteTextAndUpdateSpanCursor() {
        if (DEBUG) Log.i(TAG, "clearAutocompleteAndUpdateSpanCursor");
        clearAutocompleteText();
        // Take effect and notify if not already in a batch edit.
        if (mInputConnection != null) {
            mInputConnection.onBeginImeCommand();
            mInputConnection.onEndImeCommand();
        } else {
            mSpanCursorController.removeAutocompleteSpan();
            notifyAutocompleteTextStateChanged();
        }
    }

    // These cursor movements commit the autocomplete and apply the movement.
    private boolean cursorMovementCommitsAutocomplete(final KeyEvent event) {
        int code = event.getKeyCode();
        return code == KeyEvent.KEYCODE_MOVE_END
                || code == KeyEvent.KEYCODE_MOVE_HOME
                || code == KeyEvent.KEYCODE_DPAD_LEFT
                || code == KeyEvent.KEYCODE_DPAD_RIGHT;
    }

    @Override
    public boolean dispatchKeyEvent(final KeyEvent event) {
        if (DEBUG) Log.i(TAG, "dispatchKeyEvent");
        if (mInputConnection == null) {
            return mDelegate.super_dispatchKeyEvent(event);
        }

        boolean retVal;
        mInputConnection.onBeginImeCommand();
        if (hasAutocomplete() && event.getAction() == KeyEvent.ACTION_DOWN) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_FORWARD_DEL) {
                // The editor doesn't see the selected text so won't handle forward delete.
                clearAutocompleteText();
                mLastEditWasTyping = false;

                retVal = true;
            } else if (cursorMovementCommitsAutocomplete(event)) {
                // These commands treat the autocomplete suggestion as a selection and then apply
                // the cursor movement.
                int currentPos = mCurrentState.getSelStart();
                int totalLength = mCurrentState.getUserText().length();
                if (mCurrentState.getAutocompleteText().isPresent()) {
                    totalLength += mCurrentState.getAutocompleteText().get().length();
                }

                mInputConnection.commitAutocomplete();
                mDelegate.setSelection(currentPos, totalLength);
                retVal = mDelegate.super_dispatchKeyEvent(event);
            } else if (event.getKeyCode() == KeyEvent.KEYCODE_TAB) {
                mInputConnection.commitAutocomplete();
                retVal = true;
            } else {
                // It might make sense to commit the autocomplete text here but the
                // AutocompleteMediator queries us via getTextWithAutocomplete() so it's included
                // either way. Avoiding the extra commit eliminates a brief cursor flash at the end
                // of the autocomplete suggestion.
                retVal = mDelegate.super_dispatchKeyEvent(event);
            }
        } else {
            if (event.getAction() == KeyEvent.ACTION_DOWN
                    && event.getKeyCode() == KeyEvent.KEYCODE_FORWARD_DEL) {
                // Delete key when there's no autocomplete suggestion. Use the normal behavior but
                // inhibit suggestions.
                mLastEditWasTyping = false;
            }
            retVal = mDelegate.super_dispatchKeyEvent(event);
        }

        mInputConnection.onEndImeCommand();
        return retVal;
    }

    @Override
    public void onSetText(CharSequence text) {
        if (DEBUG) Log.i(TAG, "onSetText: " + text);
        // setText() does not necessarily trigger onTextChanged(). We need to accept the new text
        // and reset the states.
        mCurrentState.set(
                text.toString(), Optional.empty(), Optional.empty(), text.length(), text.length());
        mSpanCursorController.reset();
        mPreviouslyNotifiedState.copyFrom(mCurrentState);
        mPreviouslySetState.copyFrom(mCurrentState);
        if (mBatchEditNestCount == 0) updateSelectionForTesting();
    }

    @Override
    public void onSelectionChanged(int selStart, int selEnd) {
        if (DEBUG) Log.i(TAG, "onSelectionChanged [%d,%d]", selStart, selEnd);
        if (mCurrentState.getSelStart() == selStart && mCurrentState.getSelEnd() == selEnd) return;

        // Do not allow users to select the space between additional texts.
        int maxLength =
                mCurrentState.getUserText().length()
                        + mCurrentState.getAutocompleteText().map(t -> t.length()).orElse(0);
        if (selStart > maxLength || selEnd > maxLength) {
            int newStart = selStart > maxLength ? maxLength : selStart;
            int newEnd = selEnd > maxLength ? maxLength : selEnd;
            mDelegate.setSelection(newStart, newEnd);
            return;
        }

        mCurrentState.setSelection(selStart, selEnd);
        if (mBatchEditNestCount > 0) return;
        int len = mCurrentState.getUserText().length();
        if (mCurrentState.getAutocompleteText().isPresent()) {
            if (selStart > len || selEnd > len) {
                if (DEBUG) Log.i(TAG, "Autocomplete text is being touched. Make it real.");
                if (mInputConnection != null) mInputConnection.commitAutocomplete();
            } else {
                if (DEBUG) Log.i(TAG, "Touching before the cursor removes autocomplete.");
                clearAutocompleteTextAndUpdateSpanCursor();
            }
        }
        updateSelectionForTesting();
        notifyAutocompleteTextStateChanged();
    }

    @Override
    public void onFocusChanged(boolean focused) {
        if (DEBUG) Log.i(TAG, "onFocusChanged: " + focused);

        if (!focused) {
            // Reset selection now. It will be updated immediately after focus is re-gained.
            // We do this to ensure the selection changed announcements are advertised by us
            // since we suppress all TEXT_SELECTION_CHANGED announcements coming from EditText.
            mPreviouslyNotifiedState.setSelection(-1, -1);
            mCurrentState.setSelection(-1, -1);
        }
    }

    @Override
    public void onTextChanged(CharSequence text, int start, int beforeLength, int afterLength) {
        if (DEBUG) Log.i(TAG, "onTextChanged: " + text);
        mSpanCursorController.reflectTextUpdateInState(mCurrentState, text);
        if (mBatchEditNestCount > 0) return; // let endBatchEdit() handles changes from IME.
        // An external change such as text paste occurred.
        mLastEditWasTyping = false;
        clearAutocompleteTextAndUpdateSpanCursor();
    }

    @Override
    public void onPaste() {
        if (DEBUG) Log.i(TAG, "onPaste");
    }

    @Override
    public String getTextWithAutocomplete() {
        String retVal = mCurrentState.getText();
        if (DEBUG) Log.i(TAG, "getTextWithAutocomplete: %s", retVal);
        return retVal;
    }

    @Override
    public String getTextWithoutAutocomplete() {
        String retVal = mCurrentState.getUserText();
        if (DEBUG) Log.i(TAG, "getTextWithoutAutocomplete: " + retVal);
        return retVal;
    }

    @Override
    public int getAutocompleteTextLength() {
        return mCurrentState.getAutocompleteText().map(t -> t.length()).orElse(0);
    }

    @Override
    public Optional<String> getAdditionalText() {
        return mCurrentState.getAdditionalText();
    }

    @Override
    public void setIgnoreTextChangeFromAutocomplete(boolean ignore) {
        if (DEBUG) Log.i(TAG, "setIgnoreText: " + ignore);
        mIgnoreTextChangeFromAutocomplete = ignore;
    }

    @Override
    public void setAutocompleteText(
            @NonNull CharSequence userText,
            @Nullable CharSequence inlineAutocompleteText,
            Optional<String> additionalText) {
        // Note: this is invoked when the Autocomplete text is supplied by the Autocomplete
        // subsystem. These changes should be ignored for Autocomplete, specifically should not
        // be sent back to the Autocomplete subsystem to trigger suggestions fetch.
        setIgnoreTextChangeFromAutocomplete(true);
        setAutocompleteTextInternal(
                userText.toString(),
                inlineAutocompleteText != null ? inlineAutocompleteText.toString() : null,
                additionalText);
        setIgnoreTextChangeFromAutocomplete(false);
    }

    private void setAutocompleteTextInternal(
            @NonNull String userText,
            @Nullable String autocompleteText,
            Optional<String> additionalText) {
        if (DEBUG) Log.i(TAG, "setAutocompleteText: %s[%s]", userText, autocompleteText);
        mPreviouslySetState.set(
                userText,
                TextUtils.isEmpty(autocompleteText)
                        ? Optional.empty()
                        : Optional.of(autocompleteText),
                additionalText,
                userText.length(),
                userText.length());
        // TODO(changwan): avoid any unnecessary removal and addition of autocomplete text when it
        // is not changed or when it is appended to the existing autocomplete text.
        if (mInputConnection != null) {
            mInputConnection.onBeginImeCommand();
            mInputConnection.onEndImeCommand();
        }
    }

    @Override
    public boolean shouldAutocomplete() {
        boolean retVal =
                mBatchEditNestCount == 0
                        && mLastEditWasTyping
                        && mCurrentState.isCursorAtEndOfUserText()
                        && doesKeyboardSupportAutocomplete()
                        && isNonCompositionalText(getTextWithoutAutocomplete());
        if (DEBUG) Log.i(TAG, "shouldAutocomplete: " + retVal);
        return retVal;
    }

    private boolean doesKeyboardSupportAutocomplete() {
        String pkgName = mDelegate.getKeyboardPackageName();
        return !pkgName.contains(".iqqi") // crbug.com/767016
                && !pkgName.contains("omronsoft")
                && !pkgName.contains(".iwnn"); // crbug.com/758443
    }

    private boolean shouldFinishCompositionOnDeletion() {
        // crbug.com/758443, crbug.com/766888: Japanese keyboard does not finish composition when we
        // restore the deleted text, and later typing will make Japanese keyboard move before the
        // restored character. Most keyboards accept finishComposingText and update their internal
        // states.
        String pkgName = mDelegate.getKeyboardPackageName();
        // One exception is the recent version of Samsung keyboard which works goofily only
        // when we finish composing text here. Since it is more difficult to block all Japanese
        // keyboards, instead we call finishComposingText() for all the keyboards except for Samsung
        // keyboard.
        return !pkgName.contains("com.sec.android.inputmethod")
                // crbug.com/1071011: LG keyboard has the same issue.
                && !pkgName.contains("com.lge.ime");
    }

    @VisibleForTesting
    public static boolean isNonCompositionalText(String text) {
        // To start with, we are only activating this for English alphabets, European characters,
        // numbers and URLs to avoid potential bad interactions with more complex IMEs.
        // The rationale for including character sets with diacritical marks is that backspacing on
        // a letter with a diacritical mark most likely deletes the whole character instead of
        // removing the diacritical mark.
        // TODO(changwan): also scan for other traditionally non-IME charsets.
        return NON_COMPOSITIONAL_TEXT_PATTERN.matcher(text).matches();
    }

    @Override
    public boolean hasAutocomplete() {
        boolean retVal = mCurrentState.getAutocompleteText().isPresent();
        if (DEBUG) Log.i(TAG, "hasAutocomplete: " + retVal);
        return retVal;
    }

    @Override
    public InputConnection getInputConnection() {
        return mInputConnection;
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
        return mDelegateShouldIgnoreAccessibilityEvents;
    }

    @VisibleForTesting
    public AutocompleteState getCurrentAutocompleteState() {
        return mCurrentState;
    }

    @VisibleForTesting
    public class AutocompleteInputConnection extends InputConnectionWrapper {
        private final AutocompleteState mPreBatchEditState;

        public AutocompleteInputConnection() {
            super(null, true);
            mPreBatchEditState = new AutocompleteState(mCurrentState);
        }

        private boolean incrementBatchEditCount() {
            ++mBatchEditNestCount;
            // After the outermost super.beginBatchEdit(), EditText will stop selection change
            // update to the IME app.
            return super.beginBatchEdit();
        }

        private boolean decrementBatchEditCount() {
            --mBatchEditNestCount;
            boolean retVal = super.endBatchEdit();
            if (mBatchEditNestCount == 0) {
                // At the outermost super.endBatchEdit(), EditText will resume selection change
                // update to the IME app.
                updateSelectionForTesting();
            }
            return retVal;
        }

        public void commitAutocomplete() {
            if (DEBUG) Log.i(TAG, "commitAutocomplete");
            if (!hasAutocomplete()) return;

            String autocompleteText = mCurrentState.getAutocompleteText().get();

            mCurrentState.commitAutocompleteText();
            // Invalidate mPreviouslySetState.
            mPreviouslySetState.copyFrom(mCurrentState);
            mLastEditWasTyping = false;

            if (mBatchEditNestCount == 0) {
                incrementBatchEditCount(); // avoids additional notifyAutocompleteTextStateChanged()
                mSpanCursorController.commitSpan();
                decrementBatchEditCount();
            } else {
                // We have already removed span in the onBeginImeCommand(), just append the text.
                mDelegate.append(autocompleteText);
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
            if (DEBUG) Log.i(TAG, "onBeginImeCommand: " + mBatchEditNestCount);
            boolean retVal = incrementBatchEditCount();
            if (mBatchEditNestCount == 1) {
                mPreBatchEditState.copyFrom(mCurrentState);
            } else if (mDeletePostfixOnNextBeginImeCommand > 0) {
                // Note: in languages that rely on character composition, the last incomplete
                // character may not be recognized as part of the string, but it may still be
                // accounted for by the mDeletePostfixOnNextBeginImeCommand.
                // In such case, the text below is actually shorter than the user input, and the
                // computed string boundaries enter negative index space.
                int len = mDelegate.getText().length();
                if (mDeletePostfixOnNextBeginImeCommand > len) {
                    mDeletePostfixOnNextBeginImeCommand = len;
                }
                mDelegate.getText().delete(len - mDeletePostfixOnNextBeginImeCommand, len);
            }
            mDeletePostfixOnNextBeginImeCommand = 0;
            mSpanCursorController.removeAutocompleteSpan();
            return retVal;
        }

        private void restoreBackspacedText(String diff) {
            if (DEBUG) Log.i(TAG, "restoreBackspacedText. diff: " + diff);

            if (mBatchEditNestCount > 0) {
                // If batch edit hasn't finished, we will restore backspaced text only for visual
                // effects. However, for internal operations to work correctly, we need to remove
                // the restored diff at the beginning of next IME operation.
                mDeletePostfixOnNextBeginImeCommand = diff.length();
            }
            if (mBatchEditNestCount == 0) { // only at the outermost batch edit
                if (shouldFinishCompositionOnDeletion()) super.finishComposingText();
            }
            incrementBatchEditCount(); // avoids additional notifyAutocompleteTextStateChanged()
            Editable editable = mDelegate.getEditableText();
            editable.append(diff);
            decrementBatchEditCount();
        }

        private boolean setAutocompleteSpan() {
            mSpanCursorController.removeAutocompleteSpan();
            if (DEBUG) {
                Log.i(TAG, "setAutocompleteSpan. %s->%s", mPreviouslySetState, mCurrentState);
            }
            if (!mCurrentState.isCursorAtEndOfUserText()) return false;

            if (mCurrentState.reuseAutocompleteTextIfPrefixExtension(mPreviouslySetState)) {
                mSpanCursorController.setSpan(mCurrentState);
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
         * Always call this at the end of an IME command. Compare this with endBatchEdit() which is
         * by itself an IME command.
         *
         * @return {@code true} if the batch edit is still in progress. {@code false} otherwise.
         */
        public boolean onEndImeCommand() {
            if (DEBUG) Log.i(TAG, "onEndImeCommand: " + (mBatchEditNestCount - 1));
            String diff = mCurrentState.getBackwardDeletedTextFrom(mPreBatchEditState);
            if (diff != null) {
                // Update selection first such that keyboard app gets what it expects.
                boolean retVal = decrementBatchEditCount();

                if (mPreBatchEditState.getAutocompleteText().isPresent()) {
                    // Undo delete to retain the last character and only remove autocomplete text.
                    restoreBackspacedText(diff);
                }
                mLastEditWasTyping = false;
                clearAutocompleteText();
                notifyAutocompleteTextStateChanged();
                return retVal;
            }
            if (!setAutocompleteSpan()) {
                clearAutocompleteText();
            }
            boolean retVal = decrementBatchEditCount();
            // Simply typed some characters or whole text selection has been overridden.
            if (mCurrentState.isForwardTypedFrom(mPreBatchEditState)
                    || (mPreBatchEditState.isWholeUserTextSelected()
                            && mCurrentState.getUserText().length() > 0
                            && mCurrentState.isCursorAtEndOfUserText())) {
                mLastEditWasTyping = true;
            }
            notifyAutocompleteTextStateChanged();
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
}
