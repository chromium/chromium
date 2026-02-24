// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.util.Range;
import android.view.ActionMode;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

/** Coordinates the interactions with the UrlBar text component. */
@NullMarked
public class UrlBarCoordinator
        implements UrlBarEditingTextStateProvider,
                UrlFocusChangeListener,
                KeyboardVisibilityDelegate.KeyboardVisibilityListener {
    private static final int KEYBOARD_HIDE_DELAY_MS = 150;

    private final UrlBar mUrlBar;
    private final UrlBarMediator mMediator;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Callback<Boolean> mFocusChangeCallback;
    private final Callback<Boolean> mTextWrappedCallback;
    private final ObserverList<Callback<Boolean>> mTextWrapListeners = new ObserverList<>();
    private @Nullable Runnable mKeyboardHideTask;
    private boolean mHasFocus;
    private boolean mTextIsWrapped;

    /**
     * Constructs a coordinator for the given UrlBar view.
     *
     * @param context The current Android's context.
     * @param urlBar The {@link UrlBar} view this coordinator encapsulates.
     * @param windowDelegate Delegate for accessing and mutating window properties, e.g. soft input
     *     mode.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param focusChangeCallback The callback that will be notified when focus changes on the
     *     UrlBar.
     * @param delegate The primary delegate for the UrlBar view.
     * @param keyboardVisibilityDelegate Delegate that allows querying and changing the keyboard's
     *     visibility.
     * @param isIncognitoBranded Whether incognito mode is initially enabled. This can later be
     *     changed using {@link #setIncognitoColorsEnabled(boolean)}. @{@link OnLongClickListener}
     *     for the url bar.
     */
    public UrlBarCoordinator(
            Context context,
            UrlBar urlBar,
            ActionMode.@Nullable Callback actionModeCallback,
            Callback<Boolean> focusChangeCallback,
            UrlBarDelegate delegate,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            boolean isIncognitoBranded,
            @Nullable OnLongClickListener onLongClickListener) {
        mUrlBar = urlBar;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mFocusChangeCallback = focusChangeCallback;
        mTextWrappedCallback = this::onTextWrappingChanged;

        PropertyModel model =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.ACTION_MODE_CALLBACK, actionModeCallback)
                        .with(UrlBarProperties.DELEGATE, delegate)
                        .with(UrlBarProperties.INCOGNITO_COLORS_ENABLED, isIncognitoBranded)
                        .with(UrlBarProperties.LONG_CLICK_LISTENER, onLongClickListener)
                        .with(UrlBarProperties.TEXT_WRAPPED_CALLBACK, mTextWrappedCallback)
                        .build();
        PropertyModelChangeProcessor.create(model, urlBar, UrlBarViewBinder::bind);

        mMediator = new UrlBarMediator(context, model, this::onUrlFocusChangeInternal);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(this);
    }

    public void destroy() {
        mMediator.destroy();
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(this);
        if (mKeyboardHideTask != null) {
            mUrlBar.removeCallbacks(mKeyboardHideTask);
        }
        mUrlBar.destroy();
    }

    /** Returns whether the url bar currently contains more than a single line of text. */
    public boolean isTextWrapped() {
        return mTextIsWrapped;
    }

    /**
     * Adds a listener for text wrapping changes.
     *
     * @param listener The listener to be added.
     */
    public void addTextWrappingChangeListener(Callback<Boolean> listener) {
        mTextWrapListeners.addObserver(listener);
    }

    /**
     * Removes a listener for text wrapping changes.
     *
     * @param listener The listener to be removed.
     */
    public void removeTextWrappingChangeListener(Callback<Boolean> listener) {
        mTextWrapListeners.removeObserver(listener);
    }

    private void onTextWrappingChanged(boolean isWrapped) {
        mTextIsWrapped = isWrapped;
        for (Callback<Boolean> listener : mTextWrapListeners) {
            listener.onResult(isWrapped);
        }
    }

    /** Set the callback that will be invoked each time the content of the Omnibox changes. */
    public void setTextChangeListener(Callback<String> listener) {
        mMediator.setTextChangeListener(listener);
    }

    /**
     * Set the callback that will be invoked for:
     *
     * <ul>
     *   <li>All hardware keyboard sourced key events,
     *   <li>All enter key events, regardless of source.
     * </ul>
     */
    public void setKeyDownListener(View.OnKeyListener listener) {
        mMediator.setKeyDownListener(listener);
    }

    /**
     * @see UrlBarMediator#setUrlBarData(UrlBarData, int, Range<Integer>)
     */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, Range<Integer> selection) {
        return mMediator.setUrlBarData(data, scrollType, selection);
    }

    /** Returns the UrlBarData representing the current contents of the UrlBar. */
    public UrlBarData getUrlBarData() {
        return mMediator.getUrlBarData();
    }

    /**
     * @see UrlBarMediator#setAutocompleteText(String, String, String)
     */
    public void setAutocompleteText(
            String userText,
            @Nullable String autocompleteText,
            @Nullable String additionalText,
            @Nullable String siteSearchLabel) {
        mMediator.setAutocompleteText(userText, autocompleteText, additionalText, siteSearchLabel);
    }

    /**
     * @see UrlBarMediator#setBrandedColorScheme(int)
     */
    public void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        mMediator.setBrandedColorScheme(brandedColorScheme);
    }

    /**
     * @see UrlBarMediator#setIncognitoColorsEnabled(boolean)
     */
    public void setIncognitoColorsEnabled(boolean incognitoColorsEnabled) {
        mMediator.setIncognitoColorsEnabled(incognitoColorsEnabled);
    }

    /**
     * @see UrlBarMediator#setAllowFocus(boolean)
     */
    public void setAllowFocus(boolean allowFocus) {
        mMediator.setAllowFocus(allowFocus);
    }

    /**
     * @see UrlBarMediator#setUrlDirectionListener(Callback<Integer>)
     */
    public void setUrlDirectionListener(Callback<Integer> listener) {
        mMediator.setUrlDirectionListener(listener);
    }

    /**
     * @see UrlBarMediator#setIsInCct(boolean)
     */
    public void setIsInCct(boolean isInCct) {
        mMediator.setIsInCct(isInCct);
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

    @Override
    public void setSiteSearchChip(@Nullable String keyword) {
        mUrlBar.setSiteSearchChip(keyword);
    }

    /** Returns the {@link ViewRectProvider} for the UrlBar. */
    public ViewRectProvider getViewRectProvider() {
        return new ViewRectProvider(mUrlBar);
    }

    /**
     * @see UrlBar#getVisibleTextPrefixHint()
     */
    public @Nullable CharSequence getVisibleTextPrefixHint() {
        return mUrlBar.getVisibleTextPrefixHint();
    }

    // LocationBarLayout.UrlFocusChangeListener implementation.
    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    // KeyboardVisibilityDelegate.KeyboardVisibilityListener implementation.
    @Override
    public void keyboardVisibilityChanged(boolean isKeyboardShowing) {
        // The cursor visibility should follow soft keyboard visibility and should be hidden
        // when keyboard is dismissed for any reason (including scroll).
        mUrlBar.setCursorVisible(isKeyboardShowing);
    }

    /* package */ boolean hasFocus() {
        return mHasFocus;
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

    /* package */ void dispatchGoEvent() {
        if (!mHasFocus) return;
        mUrlBar.onEditorAction(EditorInfo.IME_ACTION_GO);
    }

    /**
     * Toggle showing only the origin portion of the URL (as opposed to the default behavior of
     * showing the max amount of the url, prioritizing the origin)
     */
    public void setShowOriginOnly(boolean showOriginOnly) {
        mMediator.setShowOriginOnly(showOriginOnly);
    }

    /** Toggle the url bar's text size to be small or normal sized. */
    public void setUseSmallText(boolean useSmallText) {
        mMediator.setUseSmallText(useSmallText);
    }

    /**
     * Controls keyboard visibility.
     *
     * @param showKeyboard Whether the soft keyboard should be shown.
     * @param shouldDelayHiding When true, keyboard hide operation will be delayed slightly to
     *     improve the animation smoothness.
     */
    public void setKeyboardVisibility(boolean showKeyboard, boolean shouldDelayHiding) {
        // Cancel pending jobs to prevent any possibility of keyboard flicker.
        if (mKeyboardHideTask != null) {
            mUrlBar.removeCallbacks(mKeyboardHideTask);
        }
        mKeyboardHideTask = null;

        // Note: due to nature of this mechanism, we may occasionally experience subsequent requests
        // to show or hide keyboard anyway. This may happen when we schedule keyboard hide, and
        // receive a second request to hide the keyboard instantly.
        if (showKeyboard) {
            mKeyboardVisibilityDelegate.showKeyboard(mUrlBar);
        } else {
            // The animation rendering may not yet be 100% complete and hiding the keyboard makes
            // the animation quite choppy.
            mKeyboardHideTask =
                    () -> {
                        mKeyboardVisibilityDelegate.hideKeyboard(mUrlBar);
                        mKeyboardHideTask = null;
                    };
            mUrlBar.postDelayed(mKeyboardHideTask, shouldDelayHiding ? KEYBOARD_HIDE_DELAY_MS : 0);
            // Convert the keyboard back to resize mode (delay the change for an arbitrary amount
            // of time in hopes the keyboard will be completely hidden before making this change).
        }
    }

    /**
     * @param hasSuggestions Whether suggestions are showing in the URL bar.
     */
    public void onUrlBarSuggestionsChanged(boolean hasSuggestions) {
        mMediator.onUrlBarSuggestionsChanged(hasSuggestions);
    }

    private void onUrlFocusChangeInternal(boolean hasFocus) {
        InputMethodManager imm =
                (InputMethodManager)
                        mUrlBar.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        mHasFocus = hasFocus;
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
            // Manually set that the URL bar is no longer showing suggestions when focus is lost as
            // this won't happen automatically.
            mMediator.onUrlBarSuggestionsChanged(false);
        }
        mFocusChangeCallback.onResult(hasFocus);
    }

    /** Signals that's it safe to call code that requires native to be loaded. */
    public void onFinishNativeInitialization() {
        mUrlBar.onFinishNativeInitialization();
    }

    /**
     * @see UrlBarMediator#setUrlBarHintTextColorForDefault(int)
     */
    public void setUrlBarHintTextColorForDefault(@BrandedColorScheme int brandedColorScheme) {
        mMediator.setUrlBarHintTextColorForDefault(brandedColorScheme);
    }

    /**
     * @see UrlBarMediator#setUrlBarHintTextColorForNtp()
     */
    public void setUrlBarHintTextColorForNtp() {
        mMediator.setUrlBarHintTextColorForNtp();
    }

    /**
     * @see UrlBarMediator#setUrlBarHintText(String)
     */
    public void setUrlBarHintText(String hintTextRes) {
        mMediator.setUrlBarHintText(hintTextRes);
    }
}
