// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextWatcher;
import android.view.ActionMode;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlTextChangeListener;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinates the interactions with the UrlBar text component.
 */
public class UrlBarCoordinator implements UrlBarEditingTextStateProvider, UrlFocusChangeListener,
                                          KeyboardVisibilityDelegate.KeyboardVisibilityListener {
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;
    private static final int KEYBOARD_MODE_CHANGE_DELAY_MS = 300;

    private static final Runnable NO_OP_RUNNABLE = () -> {};

    /** Specified how the text should be selected when focused. */
    @IntDef({SelectionState.SELECT_ALL, SelectionState.SELECT_END})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SelectionState {
        /** Select all of the text. */
        int SELECT_ALL = 0;

        /** Selection (along with the input cursor) will be placed at the end of the text. */
        int SELECT_END = 1;
    }

    private UrlBar mUrlBar;
    private UrlBarMediator mMediator;
    private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private WindowDelegate mWindowDelegate;
    private Runnable mKeyboardResizeModeTask = NO_OP_RUNNABLE;
    private Runnable mKeyboardHideTask = NO_OP_RUNNABLE;
    private Callback<Boolean> mFocusChangeCallback;
    private boolean mShouldShowModernizeVisualUpdate;

    /**
     * Constructs a coordinator for the given UrlBar view.
     *
     * @param urlBar The {@link UrlBar} view this coordinator encapsulates.
     * @param windowDelegate Delegate for accessing and mutating window properties, e.g. soft input
     *         mode.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param focusChangeCallback The callback that will be notified when focus changes on the
     *         UrlBar.
     * @param delegate The primary delegate for the UrlBar view.
     * @param keyboardVisibilityDelegate Delegate that allows querying and changing the keyboard's
     *         visibility.
     * @param isIncognito Whether incognito mode is initially enabled. This can later be changed
     *         using {@link #setIncognitoColorsEnabled(boolean)}.
     * @param reportExceptionCallback A {@link Callback} to report exceptions.
     */
    public UrlBarCoordinator(@NonNull UrlBar urlBar, @Nullable WindowDelegate windowDelegate,
            @NonNull ActionMode.Callback actionModeCallback,
            @NonNull Callback<Boolean> focusChangeCallback, @NonNull UrlBarDelegate delegate,
            @NonNull KeyboardVisibilityDelegate keyboardVisibilityDelegate, boolean isIncognito,
            Callback<Throwable> reportExceptionCallback) {
        mUrlBar = urlBar;
        urlBar.setTag(R.id.report_exception_callback, reportExceptionCallback);
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mWindowDelegate = windowDelegate;
        mFocusChangeCallback = focusChangeCallback;

        PropertyModel model =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.ACTION_MODE_CALLBACK, actionModeCallback)
                        .with(UrlBarProperties.WINDOW_DELEGATE, windowDelegate)
                        .with(UrlBarProperties.DELEGATE, delegate)
                        .with(UrlBarProperties.INCOGNITO_COLORS_ENABLED, isIncognito)
                        .build();
        PropertyModelChangeProcessor.create(model, urlBar, UrlBarViewBinder::bind);

        mMediator = new UrlBarMediator(model, this::onUrlFocusChangeInternal);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(this);
    }

    public void destroy() {
        mMediator.destroy();
        mMediator = null;
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(this);
        mUrlBar.removeCallbacks(mKeyboardResizeModeTask);
        mUrlBar.removeCallbacks(mKeyboardHideTask);
        mUrlBar.destroy();
        mUrlBar = null;
        mFocusChangeCallback = null;
    }

    /** @see UrlBarMediator#addUrlTextChangeListener(UrlTextChangeListener) */
    public void addUrlTextChangeListener(UrlTextChangeListener listener) {
        mMediator.addUrlTextChangeListener(listener);
    }

    /** @see TextWatcher */
    public void addTextChangedListener(TextWatcher textWatcher) {
        mMediator.addTextChangedListener(textWatcher);
    }

    /** @see UrlBarMediator#setUrlBarData(UrlBarData, int, int) */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, @SelectionState int state) {
        return mMediator.setUrlBarData(data, scrollType, state);
    }

    /** Returns the UrlBarData representing the current contents of the UrsssdddsssslBar. */
    public UrlBarData getUrlBarData() {
        return mMediator.getUrlBarData();
    }

    /** @see UrlBarMediator#setAutocompleteText(String, String) */
    public void setAutocompleteText(String userText, String autocompleteText) {
        mMediator.setAutocompleteText(userText, autocompleteText);
    }

    /** @see UrlBarMediator#setBrandedColorScheme(int) */
    public boolean setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        return mMediator.setBrandedColorScheme(brandedColorScheme);
    }

    /** @see UrlBarMediator#setIncognitoColorsEnabled(boolean) */
    public void setIncognitoColorsEnabled(boolean incognitoColorsEnabled) {
        mMediator.setIncognitoColorsEnabled(incognitoColorsEnabled);
    }

    /** @see UrlBarMediator#setAllowFocus(boolean) */
    public void setAllowFocus(boolean allowFocus) {
        mMediator.setAllowFocus(allowFocus);
    }

    /** @see UrlBarMediator#setUrlDirectionListener(Callback<Integer>) */
    public void setUrlDirectionListener(Callback<Integer> listener) {
        mMediator.setUrlDirectionListener(listener);
    }

    /** Selects all of the text of the UrlBar. */
    public void selectAll() {
        mUrlBar.selectAll();
    }

    @Override
    public int getSelectionStart() {
        return mUrlBar.getSelectionStart();
    }

    @Override
    public int getSelectionEnd() {
        return mUrlBar.getSelectionEnd();
    }

    @Override
    public boolean shouldAutocomplete() {
        return mUrlBar.shouldAutocomplete();
    }

    @Override
    public boolean wasLastEditPaste() {
        return mUrlBar.wasLastEditPaste();
    }

    @Override
    public String getTextWithAutocomplete() {
        return mUrlBar.getTextWithAutocomplete();
    }

    @Override
    public String getTextWithoutAutocomplete() {
        return mUrlBar.getTextWithoutAutocomplete();
    }

    /** @see UrlBar#getVisibleTextPrefixHint() */
    public CharSequence getVisibleTextPrefixHint() {
        return mUrlBar.getVisibleTextPrefixHint();
    }

    // LocationBarLayout.UrlFocusChangeListener implementation.
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        mUrlBar.removeCallbacks(mKeyboardResizeModeTask);
    }

    // KeyboardVisibilityDelegate.KeyboardVisibilityListener implementation.
    @Override
    public void keyboardVisibilityChanged(boolean isKeyboardShowing) {
        if (mShouldShowModernizeVisualUpdate) {
            // The cursor visibility should follow soft keyboard visibility and should be hidden
            // when keyboard is dismissed for any reason (including scroll).
            mUrlBar.setCursorVisible(isKeyboardShowing);
        }
    }

    /* package */ boolean hasFocus() {
        return mUrlBar.hasFocus();
    }

    /* package */ void requestFocus() {
        mUrlBar.requestFocus();
    }

    /* package */ void clearFocus() {
        mUrlBar.clearFocus();
    }

    /* package */ void requestAccessibilityFocus() {
        mUrlBar.requestAccessibilityFocus();
    }

    /**
     * Controls keyboard visibility.
     *
     * @param showKeyboard Whether the soft keyboard should be shown.
     * @param shouldDelayHiding When true, keyboard hide operation will be delayed slightly to
     *         improve the animation smoothness.
     */
    public void setKeyboardVisibility(boolean showKeyboard, boolean shouldDelayHiding) {
        // Cancel pending jobs to prevent any possibility of keyboard flicker.
        mUrlBar.removeCallbacks(mKeyboardHideTask);

        // Note: due to nature of this mechanism, we may occasionally experience subsequent requests
        // to show or hide keyboard anyway. This may happen when we schedule keyboard hide, and
        // receive a second request to hide the keyboard instantly.
        if (showKeyboard) {
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN, /* delay */ false);
            mKeyboardVisibilityDelegate.showKeyboard(mUrlBar);
        } else {
            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            // clang-format off
            mKeyboardHideTask = () -> {
                mKeyboardVisibilityDelegate.hideKeyboard(mUrlBar);
                mKeyboardHideTask = NO_OP_RUNNABLE;
            };
            // clang-format on
            mUrlBar.postDelayed(mKeyboardHideTask, shouldDelayHiding ? KEYBOARD_HIDE_DELAY_MS : 0);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE, /* delay */ true);
        }
    }

    /**
     * @param softInputMode The software input resize mode.
     * @param delay Delay the change in input mode.
     */
    private void setSoftInputMode(final int softInputMode, boolean delay) {
        mUrlBar.removeCallbacks(mKeyboardResizeModeTask);

        if (mWindowDelegate == null || mWindowDelegate.getWindowSoftInputMode() == softInputMode) {
            return;
        }

        if (delay) {
            mKeyboardResizeModeTask = () -> {
                mWindowDelegate.setWindowSoftInputMode(softInputMode);
                mKeyboardResizeModeTask = NO_OP_RUNNABLE;
            };
            mUrlBar.postDelayed(mKeyboardResizeModeTask, KEYBOARD_MODE_CHANGE_DELAY_MS);
        } else {
            mWindowDelegate.setWindowSoftInputMode(softInputMode);
        }
    }

    private void onUrlFocusChangeInternal(boolean hasFocus) {
        InputMethodManager imm = (InputMethodManager) mUrlBar.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (hasFocus) {
            // Explicitly tell InputMethodManager that the url bar is focused before any callbacks
            // so that it updates the active view accordingly. Otherwise, it may fail to update
            // the correct active view if ViewGroup.addView() or ViewGroup.removeView() is called
            // to update a view that accepts text input.
            imm.viewClicked(mUrlBar);
            mUrlBar.setCursorVisible(true);
        } else {
            // Moving focus away from UrlBar(EditText) to a non-editable focus holder, such as
            // ToolbarPhone, won't automatically hide keyboard app, but restart it with TYPE_NULL,
            // which will result in a visual glitch. Also, currently, we do not allow moving focus
            // directly from omnibox to web content's form field. Therefore, we hide keyboard on
            // focus blur indiscriminately here. Note that hiding keyboard may lower FPS of other
            // animation effects, but we found it tolerable in an experiment.
            if (imm.isActive(mUrlBar)) setKeyboardVisibility(false, false);
        }
        mFocusChangeCallback.onResult(hasFocus);
    }

    /** Signals that's it safe to call code that requires native to be loaded. */
    public void onFinishNativeInitialization() {
        mUrlBar.onFinishNativeInitialization();
        mShouldShowModernizeVisualUpdate =
                OmniboxFeatures.shouldShowModernizeVisualUpdate(mUrlBar.getContext());
    }
}
