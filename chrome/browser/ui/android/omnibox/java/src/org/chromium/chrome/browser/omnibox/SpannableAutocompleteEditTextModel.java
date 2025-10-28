// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.InputConnection;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.regex.Pattern;

/**
 * An autocomplete model that appends autocomplete text at the end of query/URL text as
 * SpannableString. By wrapping all the keyboard related operations in a batch edit, we can
 * effectively hide the existence of autocomplete text from keyboard.
 */
@NullMarked
public class SpannableAutocompleteEditTextModel
        implements AutocompleteEditTextModelBase, AutocompleteInputConnection.InputDelegate {
    private static final String TAG = "SpanAutocomplete";
    private static final boolean DEBUG = OmniboxFeatures.sDiagInputConnection.getValue();

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

    private @Nullable AutocompleteInputConnection mInputConnection;
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
    public @Nullable InputConnection onCreateInputConnection(
            @Nullable InputConnection inputConnection) {
        mLastUpdateSelStart = mDelegate.getSelectionStart();
        mLastUpdateSelEnd = mDelegate.getSelectionEnd();
        mBatchEditNestCount = 0;
        if (inputConnection == null) {
            if (DEBUG) Log.i(TAG, "onCreateInputConnection: null");
            mInputConnection = null;
            return null;
        }
        if (DEBUG) Log.i(TAG, "onCreateInputConnection");
        mInputConnection = new AutocompleteInputConnection(this);
        mInputConnection.setTarget(inputConnection);
        return mInputConnection;
    }

    public void setInputConnectionForTesting(AutocompleteInputConnection connection) {
        mInputConnection = connection;
    }

    private void sendAccessibilityEventForUserTextChange(
            AutocompleteState oldState, AutocompleteState newState) {
        String oldAutocompleteText = oldState.getAutocompleteText();
        String oldText = oldState.getText();
        String newText = newState.getText();
        String oldUserText = oldState.getUserText();
        String newUserText = newState.getUserText();
        int oldAutocompleteTextLen = oldAutocompleteText != null ? oldAutocompleteText.length() : 0;
        int newUserTextLen = newUserText.length();
        int oldUserTextLen = oldUserText.length();

        int addedCount;
        int removedCount;
        int fromIndex;
        if (newState.isBackwardDeletedFrom(oldState)) {
            addedCount = 0;
            removedCount = oldText.length() - newUserTextLen;
            fromIndex = newUserTextLen;
        } else if (newState.isForwardTypedFrom(oldState)) {
            addedCount = newUserTextLen - oldUserTextLen;
            removedCount = oldAutocompleteTextLen;
            fromIndex = oldUserTextLen;
        } else if (newUserText.equals(oldUserText)) {
            addedCount = 0;
            removedCount = oldAutocompleteTextLen;
            fromIndex = oldUserTextLen;
        } else {
            // Assume that the whole text has been replaced.
            addedCount = newText.length();
            removedCount = oldUserTextLen;
            fromIndex = 0;
        }

        mDelegateShouldIgnoreAccessibilityEvents = false;
        if (!oldText.equals(newText) && (addedCount != 0 || removedCount != 0)) {
            AccessibilityEvent event =
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
            event.setBeforeText(oldText);
            event.setFromIndex(fromIndex);
            event.setRemovedCount(removedCount);
            event.setAddedCount(addedCount);
            mDelegate.sendAccessibilityEvent(event);
        }

        if (oldState.getSelStart() != newState.getSelStart()
                || oldState.getSelEnd() != newState.getSelEnd()) {
            mDelegate.sendAccessibilityEvent(
                    AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED));
        }
        mDelegateShouldIgnoreAccessibilityEvents = true;
    }

    private void sendAccessibilityEventForAppendingAutocomplete(AutocompleteState newState) {
        String autocompleteText = newState.getAutocompleteText();
        if (autocompleteText == null) return;
        // Note that only text changes and selection does not change.
        AccessibilityEvent eventTextChanged =
                AccessibilityEvent.obtain(AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED);
        eventTextChanged.setBeforeText(newState.getUserText());
        eventTextChanged.setFromIndex(newState.getUserText().length());
        eventTextChanged.setRemovedCount(0);
        eventTextChanged.setAddedCount(autocompleteText.length());
        mDelegateShouldIgnoreAccessibilityEvents = false;
        mDelegate.sendAccessibilityEvent(eventTextChanged);
        mDelegateShouldIgnoreAccessibilityEvents = true;
    }

    private void notifyAccessibilityService() {
        if (mCurrentState.equals(mPreviouslyNotifiedState)) return;
        if (!AccessibilityState.isAccessibilityEnabled()) return;
        sendAccessibilityEventForUserTextChange(mPreviouslyNotifiedState, mCurrentState);
        // Read autocomplete text separately.
        sendAccessibilityEventForAppendingAutocomplete(mCurrentState);
    }

    @Override
    public void notifyAutocompleteTextStateChanged() {
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
                && (mCurrentState.getAutocompleteText() != null
                        || mPreviouslyNotifiedState.getAutocompleteText() == null)) {
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

    @Override
    public void clearAutocompleteText() {
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
                String autocompleteText = mCurrentState.getAutocompleteText();
                if (autocompleteText != null) {
                    totalLength += autocompleteText.length();
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
        mCurrentState.set(text.toString(), null, null, text.length(), text.length());
        mSpanCursorController.reset();
        if (mIgnoreTextChangeFromAutocomplete) {
            mPreviouslyNotifiedState.copyFrom(mCurrentState);
        }
        mPreviouslySetState.copyFrom(mCurrentState);
        if (mBatchEditNestCount == 0) updateSelectionForTesting();
    }

    @Override
    public void onSelectionChanged(int selStart, int selEnd) {
        if (DEBUG) Log.i(TAG, "onSelectionChanged [%d,%d]", selStart, selEnd);
        if (mCurrentState.getSelStart() == selStart && mCurrentState.getSelEnd() == selEnd) return;

        // Do not allow users to select the space between additional texts.
        String autocompleteText = mCurrentState.getAutocompleteText();
        int maxLength =
                mCurrentState.getUserText().length()
                        + (autocompleteText != null ? autocompleteText.length() : 0);
        if (selStart > maxLength || selEnd > maxLength) {
            int newStart = selStart > maxLength ? maxLength : selStart;
            int newEnd = selEnd > maxLength ? maxLength : selEnd;
            mDelegate.setSelection(newStart, newEnd);
            return;
        }

        mCurrentState.setSelection(selStart, selEnd);
        if (mBatchEditNestCount > 0) return;
        int len = mCurrentState.getUserText().length();
        if (autocompleteText != null) {
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
        return mCurrentState.getAutocompleteText() != null
                ? mCurrentState.getAutocompleteText().length()
                : 0;
    }

    @Override
    public @Nullable String getAdditionalText() {
        return mCurrentState.getAdditionalText();
    }

    @Override
    public void setIgnoreTextChangeFromAutocomplete(boolean ignore) {
        if (DEBUG) Log.i(TAG, "setIgnoreText: " + ignore);
        mIgnoreTextChangeFromAutocomplete = ignore;
    }

    @Override
    public void setAutocompleteText(
            CharSequence userText,
            @Nullable CharSequence inlineAutocompleteText,
            @Nullable String additionalText) {
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
            String userText, @Nullable String autocompleteText, @Nullable String additionalText) {
        if (DEBUG) Log.i(TAG, "setAutocompleteText: %s[%s]", userText, autocompleteText);
        mPreviouslySetState.set(
                userText,
                TextUtils.isEmpty(autocompleteText) ? null : autocompleteText,
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

    @Override
    public boolean shouldFinishCompositionOnDeletion() {
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
        boolean retVal = mCurrentState.getAutocompleteText() != null;
        if (DEBUG) Log.i(TAG, "hasAutocomplete: " + retVal);
        return retVal;
    }

    @Override
    public @Nullable InputConnection getInputConnection() {
        return mInputConnection;
    }

    @Override
    public void updateSelectionForTesting() {
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

    // ============================================================================================
    // AutocompleteEditTextModelBase.InputDelegate
    // ============================================================================================
    //
    @Override
    public boolean isInBatchEdit() {
        return mBatchEditNestCount > 0;
    }

    @Override
    public boolean isInFirstBatchEdit() {
        return mBatchEditNestCount == 1;
    }

    @Override
    public void incrementBatchEditCount() {
        ++mBatchEditNestCount;
    }

    @Override
    public void decrementBatchEditCount() {
        --mBatchEditNestCount;
    }

    @Override
    public AutocompleteState getCurrentState() {
        return mCurrentState;
    }

    @Override
    public AutocompleteState getPreviouslySetState() {
        return mPreviouslySetState;
    }

    @Override
    public SpanCursorController getSpanCursorController() {
        return mSpanCursorController;
    }

    @Override
    public void setLastEditWasTyping(boolean wasTyping) {
        mLastEditWasTyping = wasTyping;
    }

    @Override
    public Delegate getAutocompleteEditTextModelBaseDelegate() {
        return mDelegate;
    }

    @Override
    public int getDeletePostfixOnNextBeginImeCommand() {
        return mDeletePostfixOnNextBeginImeCommand;
    }

    @Override
    public void setDeletePostfixOnNextBeginImeCommand(int postfix) {
        mDeletePostfixOnNextBeginImeCommand = postfix;
    }
}
