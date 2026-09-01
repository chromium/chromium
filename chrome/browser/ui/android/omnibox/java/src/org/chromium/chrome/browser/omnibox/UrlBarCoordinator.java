// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.view.ActionMode;
import android.view.KeyEvent;
import android.view.View.OnKeyListener;
import android.view.View.OnLongClickListener;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Coordinates the interactions with the UrlBar text component. */
@NullMarked
public class UrlBarCoordinator
        implements UrlBarEditingTextStateProvider,
                UrlFocusChangeListener,
                KeyboardVisibilityDelegate.KeyboardVisibilityListener {

    @IntDef({
        KeyboardState.HIDDEN,
        KeyboardState.HIDING,
        KeyboardState.SHOWING,
        KeyboardState.SHOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    @interface KeyboardState {
        int HIDDEN = 0;
        int HIDING = 1;
        int SHOWING = 2;
        int SHOWN = 3;
    }

    private static final int KEYBOARD_HIDE_DELAY_MS = 150;
    private static final int KEYBOARD_DEBOUNCE_DELAY_MS = 150;

    private final UrlBar mUrlBar;
    private final UrlBarMediator mMediator;
    private final PropertyModel mModel;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Callback<UrlBarFocusChangeInfo> mFocusChangeCallback;
    private final Callback<Boolean> mTextWrappedCallback;
    private final ObserverList<Callback<Boolean>> mTextWrapListeners = new ObserverList<>();
    private final Runnable mKeyboardTransitionRunnable = this::resolveKeyboardTransition;
    private @Nullable Runnable mKeyboardHideTask;
    private @KeyboardState int mKeyboardState = KeyboardState.HIDDEN;
    private boolean mHasFocus;
    private boolean mTextIsWrapped;

    /**
     * Constructs a coordinator for the given UrlBar view.
     *
     * @param context The current Android's context.
     * @param urlBar The {@link UrlBar} view this coordinator encapsulates.
     * @param actionModeCallback Callback to handle changes in contextual action Modes.
     * @param focusChangeCallback The callback that will be notified when focus changes on the
     *     UrlBar.
     * @param delegate The primary delegate for the UrlBar view.
     * @param keyboardVisibilityDelegate Delegate that allows querying and changing the keyboard's
     *     visibility.
     * @param isIncognitoBranded Whether incognito mode is initially enabled. This can later be
     *     changed using {@link #setIncognitoColorsEnabled(boolean)}.
     * @param onLongClickListener The listener for long clicks.
     * @param textChangeListener The listener for text changes. Invoked every time omnibox content
     *     changes and used for autocomplete.
     * @param richTextChangeListener The listener for rich text changes. Invoked on each keypress.
     *     Used for site-search triggering.
     * @param keyDownListener The listener for key down events.
     */
    public UrlBarCoordinator(
            Context context,
            UrlBar urlBar,
            ActionMode.@Nullable Callback actionModeCallback,
            Callback<UrlBarFocusChangeInfo> focusChangeCallback,
            UrlBarDelegate delegate,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            boolean isIncognitoBranded,
            @Nullable OnLongClickListener onLongClickListener,
            @Nullable Callback<String> textChangeListener,
            @Nullable Callback<UrlBarTextChangeInfo> richTextChangeListener,
            @Nullable OnKeyListener keyDownListener) {
        mUrlBar = urlBar;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mFocusChangeCallback = focusChangeCallback;
        mTextWrappedCallback = this::onTextWrappingChanged;

        mModel =
                new PropertyModel.Builder(UrlBarProperties.ALL_KEYS)
                        .with(UrlBarProperties.ACTION_MODE_CALLBACK, actionModeCallback)
                        .with(UrlBarProperties.DELEGATE, delegate)
                        .with(UrlBarProperties.INCOGNITO_COLORS_ENABLED, isIncognitoBranded)
                        .with(UrlBarProperties.KEY_DOWN_LISTENER, keyDownListener)
                        .with(UrlBarProperties.LONG_CLICK_LISTENER, onLongClickListener)
                        .with(UrlBarProperties.TEXT_WRAPPED_CALLBACK, mTextWrappedCallback)
                        .with(
                                UrlBarProperties.FOCUS_CHANGE_CALLBACK,
                                this::onUrlFocusChangeInternal)
                        .build();
        PropertyModelChangeProcessor.create(mModel, urlBar, UrlBarViewBinder::bind);

        mMediator = new UrlBarMediator(context, mModel, textChangeListener, richTextChangeListener);
        mKeyboardState =
                mKeyboardVisibilityDelegate.isKeyboardShowing(urlBar)
                        ? KeyboardState.SHOWN
                        : KeyboardState.HIDDEN;
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(this);
    }

    public void destroy() {
        mMediator.destroy();
        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(this);
        if (mKeyboardHideTask != null) {
            mUrlBar.removeCallbacks(mKeyboardHideTask);
            mKeyboardHideTask = null;
        }
        mUrlBar.removeCallbacks(mKeyboardTransitionRunnable);
        mKeyboardState = KeyboardState.HIDDEN;
        mUrlBar.destroy();
    }

    /** Signals that the Omnibox input session has begun. */
    public void beginInput(FuseboxSessionState sessionState) {
        mMediator.beginInput(sessionState);
    }

    /** Signals that the Omnibox input session has ended. */
    public void endInput() {
        mMediator.endInput();
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

    /**
     * @see UrlBarMediator#setUrlBarData(UrlBarData, int, TextSelection)
     */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, TextSelection selection) {
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

    /** Sets whether this {@link UrlBar} should enable bounds ellipsis. */
    public void setBoundsEllipsisEnabled(boolean enabled) {
        mUrlBar.setBoundsEllipsisEnabled(enabled);
    }

    /** Sets the accessibility warning text. */
    public void setAccessibilityWarning(@Nullable String warning) {
        mMediator.setAccessibilityWarning(warning);
    }

    /** Set the state of "Always Show AI Mode" option. */
    public void setShowAiMode(boolean showAiMode) {
        mModel.set(UrlBarProperties.AI_MODE_PREF_ENABLED, showAiMode);
    }

    /** Set the callback when "Always Show AI Mode" is toggled. */
    public void setShowAiModeCallback(@Nullable Callback<Boolean> callback) {
        mModel.set(UrlBarProperties.AI_MODE_PREF_TOGGLE_CALLBACK, callback);
    }

    /**
     * Clears text selection, which also has the side effect of dismissing the Android selection
     * handles and context menu if showing.
     */
    public void clearTextSelection() {
        mUrlBar.clearTextSelection();
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
        if (OmniboxFeatures.isDebounceKeyboardVisibilityEnabled()) {
            // When the OS notifies us that the keyboard visibility has changed (e.g. user
            // dismissed via back gesture or IME completed showing), any pending debounce
            // transition is obsolete because the OS has reached a steady state. Clear the
            // pending transition task and synchronize our internal state with reality.
            mUrlBar.removeCallbacks(mKeyboardTransitionRunnable);
            mKeyboardState = isKeyboardShowing ? KeyboardState.SHOWN : KeyboardState.HIDDEN;
        }
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
        if (OmniboxFeatures.isDebounceKeyboardVisibilityEnabled()) {
            setKeyboardVisibilityDebounced(showKeyboard);
            return;
        }

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

    private void setKeyboardVisibilityDebounced(boolean showKeyboard) {
        boolean isCurrentlyShowing =
                mKeyboardState == KeyboardState.SHOWN || mKeyboardState == KeyboardState.SHOWING;
        if (showKeyboard == isCurrentlyShowing) {
            return;
        }

        // If we are currently in a transiting state (HIDING or SHOWING) and a request in the
        // opposite direction arrives, the OS was never actually instructed to change visibility
        // (the debounce timer hasn't fired yet). We can fast-cancel the pending task and
        // immediately transition back to the corresponding steady state (SHOWN or HIDDEN)
        // without calling into Android's InputMethodManager.
        if (mKeyboardState == KeyboardState.HIDING || mKeyboardState == KeyboardState.SHOWING) {
            mUrlBar.removeCallbacks(mKeyboardTransitionRunnable);
            mKeyboardState = showKeyboard ? KeyboardState.SHOWN : KeyboardState.HIDDEN;
            return;
        }

        mKeyboardState = showKeyboard ? KeyboardState.SHOWING : KeyboardState.HIDING;
        mUrlBar.postDelayed(mKeyboardTransitionRunnable, KEYBOARD_DEBOUNCE_DELAY_MS);
    }

    private void resolveKeyboardTransition() {
        if (mKeyboardState == KeyboardState.SHOWING) {
            mKeyboardVisibilityDelegate.showKeyboard(mUrlBar);
        } else if (mKeyboardState == KeyboardState.HIDING) {
            mKeyboardVisibilityDelegate.hideKeyboard(mUrlBar);
        }
    }

    /**
     * @param hasSuggestions Whether suggestions are showing in the URL bar.
     */
    public void onUrlBarSuggestionsChanged(boolean hasSuggestions) {
        mMediator.onUrlBarSuggestionsChanged(hasSuggestions);
    }

    private void onUrlFocusChangeInternal(UrlBarFocusChangeInfo info) {
        if (mMediator.isReparenting()) return;
        boolean hasFocus = info.hasFocus;
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
            if (imm.isActive(mUrlBar)) {
                setKeyboardVisibility(/* showKeyboard= */ false, /* shouldDelayHiding= */ false);
            }
            // Manually set that the URL bar is no longer showing suggestions when focus is lost as
            // this won't happen automatically.
            mMediator.onUrlBarSuggestionsChanged(false);
        }

        mFocusChangeCallback.onResult(info);
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
     * @see UrlBarMediator#setUrlBarHintText(CharSequence)
     */
    public void setUrlBarHintText(CharSequence hintTextRes) {
        mMediator.setUrlBarHintText(hintTextRes);
    }

    /**
     * Tell the UrlBar that it is being relocated to a new parent. Focus change notifications are
     * dropped while this process is ongoing.
     */
    public void startReparenting() {
        mMediator.startReparenting(
                new TextSelection(mUrlBar.getSelectionStart(), mUrlBar.getSelectionEnd()));
    }

    /**
     * Tell the UrlBar that it has been relocated to a new parent and set its new focus state.
     *
     * @param postReparentingFocus Whether the UrlBar should be focused now that the reparenting
     *     process has completed.
     */
    public void finishReparenting(boolean postReparentingFocus) {
        if (postReparentingFocus) {
            mUrlBar.requestFocus();
        } else {
            mUrlBar.clearFocus();
        }
        mMediator.finishReparenting(postReparentingFocus);
        if (mHasFocus != postReparentingFocus) {
            onUrlFocusChangeInternal(
                    new UrlBarFocusChangeInfo(
                            postReparentingFocus, UrlBarFocusChangeInfo.NO_FOCUS_DIRECTION));
        }
    }

    public void maybeAcceptInlineSuggestion(KeyEvent event) {
        mUrlBar.maybeAcceptInlineSuggestion(event);
    }

    /** Returns whether the url bar has inline autocomplete text. */
    public boolean hasAutocomplete() {
        return mUrlBar.hasAutocomplete();
    }
}
