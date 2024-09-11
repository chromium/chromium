// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.os.SystemClock;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.activity.ComponentActivity;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemViewInfo;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.base.ActionChipsProperties;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Function;

/** Utility methods and classes for testing the Omnibox. */
public class OmniboxTestUtils {
    /** Value indicating that the index is not valid. */
    public static final int SUGGESTION_INDEX_INVALID = -1;

    private final @NonNull Activity mActivity;
    private final @NonNull LocationBarLayout mLocationBar;
    private final @NonNull AutocompleteCoordinator mAutocomplete;
    private final @NonNull UrlBar mUrlBar;
    private final @NonNull Instrumentation mInstrumentation;
    private final @Nullable ToolbarLayout mToolbar;

    /**
     * Invokes a specific ViewAction on an {@link
     * org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxAction} at specific position.
     *
     * <p>This class can be chained with {@link
     * androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition}.
     */
    private static class ActionOnOmniboxActionAtPosition implements ViewAction {
        private final ViewAction mAction;

        public ActionOnOmniboxActionAtPosition(int position, ViewAction action) {
            mAction = actionOnItemAtPosition(position, action);
        }

        @Override
        public Matcher<View> getConstraints() {
            return withId(R.id.omnibox_actions_carousel);
        }

        @Override
        public String getDescription() {
            return mAction.getDescription();
        }

        @Override
        public void perform(UiController uiController, View view) {
            mAction.perform(uiController, view.findViewById(R.id.omnibox_actions_carousel));
        }
    }

    /**
     * Create a ViewAction that can be executed on a Suggestion with Action Chips.
     *
     * @param position the index of an {@link
     *         org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxAction},
     * @param action the action to perform.
     */
    public static ViewAction actionOnOmniboxActionAtPosition(int position, ViewAction action) {
        return new ActionOnOmniboxActionAtPosition(position, action);
    }

    /**
     * Class describing individual suggestion, delivering access to broad range of information.
     *
     * @param <T> The type of suggestion view.
     */
    public static class SuggestionInfo<T extends View> {
        public final int index;
        public final @OmniboxSuggestionUiType int type;
        public final @NonNull AutocompleteMatch suggestion;
        public final @NonNull PropertyModel model;
        public final @NonNull T view;

        protected SuggestionInfo(
                int index,
                @OmniboxSuggestionUiType int type,
                @NonNull AutocompleteMatch suggestion,
                @NonNull PropertyModel model,
                @NonNull T view) {
            this.index = index;
            this.type = type;
            this.suggestion = suggestion;
            this.model = model;
            this.view = view;
        }
    }

    /**
     * Create a new OmniboxTestUtils instance from supplied activity.
     *
     * This method should be called if the caller intends to retain the instance
     * for a longer period of time.
     * For short or single-time uses, consider calling static method below.
     */
    public OmniboxTestUtils(@NonNull Activity activity) {
        mActivity = activity;
        if (activity instanceof SearchActivity) {
            mLocationBar = mActivity.findViewById(R.id.search_location_bar);
            mToolbar = null;
        } else {
            mLocationBar = mActivity.findViewById(R.id.location_bar);
            mToolbar = mActivity.findViewById(R.id.toolbar);
        }
        mAutocomplete = mLocationBar.getAutocompleteCoordinator();
        mUrlBar = mActivity.findViewById(R.id.url_bar);
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
    }

    /** Disables any live autocompletion, making Omnibox behave like a standard text field. */
    public void disableLiveAutocompletion() {
        ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setTextChangeListener(null));
    }

    /**
     * Waits for all the animations to complete. Allows any preceding operation to kick off an
     * animation.
     */
    public void waitAnimationsComplete() {
        // Note: SearchActivity has no toolbar and no animations, but we still need to
        // give keyboard a bit of time to pop up (requested with delay).
        do {
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        } while (mToolbar != null && mToolbar.isAnimationRunningForTesting());
    }

    /**
     * Check that the Omnibox reaches the expected focus state.
     *
     * <p>Note: this is known to cause issues with tests that run animations.
     *
     * @param active Whether the Omnibox is expected to have focus or not.
     */
    public void checkFocus(boolean active) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "unexpected Omnibox focus state",
                            mUrlBar.hasFocus(),
                            Matchers.is(active));
                    InputMethodManager imm =
                            (InputMethodManager)
                                    mUrlBar.getContext()
                                            .getSystemService(Context.INPUT_METHOD_SERVICE);
                    Criteria.checkThat(
                            "Keyboard did not reach expected state",
                            imm.isActive(mUrlBar),
                            Matchers.is(active));
                });
    }

    /**
     * Determines whether the UrlBar currently has focus.
     *
     * @return Whether the UrlBar has focus.
     */
    public boolean getFocus() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.hasFocus());
    }

    /** Request the Omnibox focus and wait for soft keyboard to show. */
    public void requestFocus() {
        // During early startup (before completion of its first onDraw), the UrlBar
        // is not focusable. Tests have to wait for that to happen before trying to focus it.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat("Omnibox not shown.", mUrlBar.isShown(), Matchers.is(true));
                    Criteria.checkThat(
                            "Omnibox not focusable.", mUrlBar.isFocusable(), Matchers.is(true));
                    if (!mUrlBar.hasFocus()) mUrlBar.requestFocus();
                    Criteria.checkThat(
                            "Omnibox is focused.", mUrlBar.hasFocus(), Matchers.is(true));
                });
    }

    /**
     * Clear the Omnibox focus and wait until keyboard is dismissed. Performs no action if the
     * Omnibox is already unfocused.
     */
    public void clearFocus() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mUrlBar.hasFocus()) {
                        ((ComponentActivity) mActivity)
                                .getOnBackPressedDispatcher()
                                .onBackPressed();
                    }
                });
        // Needed to complete scrolling the UrlBar to TLD.
        mInstrumentation.waitForIdleSync();
        checkFocus(false);
    }

    /**
     * Set the suggestions to the Omnibox to display.
     *
     * @param autocompleteResult The set of suggestions will be displayed on the Omnibox dropdown
     *     list.
     */
    public void setSuggestions(AutocompleteResult autocompleteResult) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OnSuggestionsReceivedListener listener =
                            mAutocomplete.getSuggestionsReceivedListenerForTest();
                    listener.onSuggestionsReceived(autocompleteResult, true);
                });
    }

    /** Waits for a non-empty list of omnibox suggestions to be shown. */
    public void checkSuggestionsShown() {
        checkSuggestionsShown(true);
    }

    /** Waits for a non-empty list of omnibox suggestions to be {@link shown}. */
    public void checkSuggestionsShown(boolean shown) {
        CriteriaHelper.pollUiThread(
                () -> {
                    OmniboxSuggestionsDropdown suggestionsDropdown =
                            mLocationBar
                                    .getAutocompleteCoordinator()
                                    .getSuggestionsDropdownForTest();
                    Criteria.checkThat(
                            "suggestion list is null",
                            suggestionsDropdown,
                            Matchers.notNullValue());
                    if (shown) {
                        Criteria.checkThat(
                                "suggestion list is not shown",
                                suggestionsDropdown.getViewGroup().isShown(),
                                Matchers.is(true));
                        Criteria.checkThat(
                                "suggestion list has no entries",
                                suggestionsDropdown.getDropdownItemViewCountForTest(),
                                Matchers.greaterThan(0));
                    } else {
                        Criteria.checkThat(
                                "suggestion list is shown",
                                suggestionsDropdown.getViewGroup().isShown(),
                                Matchers.is(false));
                        Criteria.checkThat(
                                "suggestion list has entries",
                                suggestionsDropdown.getDropdownItemViewCountForTest(),
                                Matchers.equalTo(0));
                    }
                });
    }

    /**
     * Stops any subsequent AutocompleteResults from being generated.
     * Ensures that no subsequent asynchronous AutocompleteResults could tamper with test execution.
     */
    public void waitForAutocomplete() {
        AtomicLong previousId = new AtomicLong(-1);
        AtomicLong count = new AtomicLong();

        CriteriaHelper.pollUiThread(
                () -> {
                    long currentId = mAutocomplete.getCurrentNativeAutocompleteResult();
                    // Suggestions have changed as a result of a recent push.
                    // Reset the counter and monitor for possible updates.
                    if (currentId != previousId.get()) {
                        previousId.set(currentId);
                        count.set(0);
                        return false;
                    }

                    // Check that nothing has changed 3 times in a row, rejecting everything that
                    // arrives late. This guarantees that the suggestions will not change and the
                    // list can be used for testing purposes.
                    if (count.incrementAndGet() < 3) return false;
                    mAutocomplete.stopAutocompleteForTest(false);
                    return true;
                });
    }

    /**
     * Return the first suggestion of the specific type.
     *
     * @param type The type of suggestion to check.
     */
    public <T extends View> SuggestionInfo<T> findSuggestionWithType(
            @OmniboxSuggestionUiType int type) {
        return findSuggestion(info -> info.type == type);
    }

    /** Return the first suggestion that features Action Chips. */
    public @Nullable <T extends View> SuggestionInfo<T> findSuggestionWithActionChips() {
        return findSuggestion(
                info -> {
                    if (!info.model
                            .getAllSetProperties()
                            .contains(ActionChipsProperties.ACTION_CHIPS)) {
                        return false;
                    }
                    return info.model.get(ActionChipsProperties.ACTION_CHIPS) != null;
                });
    }

    /**
     * Return the first suggestion that meets requirements set by supplied filter.
     *
     * @param filter The filter to use to identify appropriate suggestion type.
     */
    public @Nullable <T extends View> SuggestionInfo<T> findSuggestion(
            @NonNull Function<DropdownItemViewInfo, Boolean> filter) {
        checkSuggestionsShown();
        AtomicReference<SuggestionInfo<T>> result = new AtomicReference<>();

        CriteriaHelper.pollUiThread(
                () -> {
                    OmniboxSuggestionsDropdown dropdown =
                            mLocationBar
                                    .getAutocompleteCoordinator()
                                    .getSuggestionsDropdownForTest();

                    ModelList currentModels =
                            mLocationBar
                                    .getAutocompleteCoordinator()
                                    .getSuggestionModelListForTest();
                    for (int i = 0; i < currentModels.size(); i++) {
                        DropdownItemViewInfo info = (DropdownItemViewInfo) currentModels.get(i);
                        T view = (T) dropdown.getDropdownItemViewForTest(i);
                        if (filter.apply(info) && view != null) {
                            result.set(
                                    new SuggestionInfo<T>(
                                            i,
                                            info.type,
                                            mAutocomplete.getSuggestionAt(i),
                                            info.model,
                                            view));
                            return true;
                        }
                    }
                    return false;
                });

        return result.get();
    }

    /**
     * Retrieve the Suggestion View for specific suggestion index. Traverses the Suggestions list
     * and skips over the Headers.
     *
     * @param <T> The type of the expected view. Inferred from call.
     * @param indexOfSuggestion The index of the suggestion view (not including the headers).
     * @return The View corresponding to suggestion with specific index, or null if there's no such
     *     suggestion.
     */
    private @Nullable <T extends View> T getSuggestionViewForIndex(int indexOfSuggestion) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OmniboxSuggestionsDropdown dropdown =
                            mLocationBar
                                    .getAutocompleteCoordinator()
                                    .getSuggestionsDropdownForTest();
                    int numViews = dropdown.getDropdownItemViewCountForTest();
                    int nonHeaderViewIndex = 0;

                    for (int childIndex = 0; childIndex < numViews; childIndex++) {
                        View view = dropdown.getDropdownItemViewForTest(childIndex);
                        if (view instanceof HeaderView) continue;

                        if (nonHeaderViewIndex == indexOfSuggestion) return (T) view;
                        nonHeaderViewIndex++;
                    }

                    return null;
                });
    }

    /**
     * Type text in the Omnibox. Requires that the Omnibox is focused ahead of call.
     *
     * @param text Text to be "typed" in the Omnibox.
     * @param execute Whether to perform the default action after typing text (ie. press the "go"
     *     button/enter key).
     */
    public void typeText(String text, boolean execute) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> KeyUtils.typeTextIntoView(mInstrumentation, mUrlBar, text));

        if (execute) sendKey(KeyEvent.KEYCODE_ENTER);
    }

    /**
     * Send key event to the Omnibox. Requires that the Omnibox is focused.
     *
     * @param keyCode The Key code to send to the Omnibox.
     */
    public void sendKey(int keyCode) {
        sendKey(keyCode, 0);
    }

    /**
     * Send key event to the Omnibox. Requires that the Omnibox is focused.
     *
     * @param keyCode The Key code to send to the Omnibox.
     * @param modifiers Additional modifiers pressed with the key (shift, alt, ...).
     */
    public void sendKey(int keyCode, int modifiers) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var currentTime = SystemClock.uptimeMillis();
                    var event =
                            new KeyEvent(
                                    /* downTime= */ currentTime,
                                    /* eventTime= */ currentTime,
                                    KeyEvent.ACTION_DOWN,
                                    keyCode,
                                    /* repeat= */ 0,
                                    modifiers);
                    if (!mUrlBar.dispatchKeyEventPreIme(event)) mUrlBar.dispatchKeyEvent(event);

                    event =
                            new KeyEvent(
                                    /* downTime= */ currentTime,
                                    /* eventTime= */ currentTime,
                                    KeyEvent.ACTION_UP,
                                    keyCode,
                                    /* repeat= */ 0,
                                    modifiers);

                    if (!mUrlBar.dispatchKeyEventPreIme(event)) mUrlBar.dispatchKeyEvent(event);
                });
    }

    /**
     * Specify the text to be shown in the Omnibox. Cancels all autocompletion. Use this to
     * initialize the state of the Omnibox, but avoid using this to validate any behavior.
     *
     * @param userText The text to be shown in the Omnibox.
     */
    public void setText(String userText) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUrlBar.setText(userText);
                    // Push this to the model as well.
                    mUrlBar.setAutocompleteText(userText, "", Optional.empty());
                });
        checkText(Matchers.equalTo(userText), null);
    }

    /**
     * Commit text to the Omnibox, as if it was supplied by the soft keyboard
     * autocorrect/autocomplete feature.
     *
     * @param textToCommit The text to supply as if it was supplied by Soft Keyboard.
     * @param commitAsAutocomplete Whether the text should be applied as autocompletion (true) or
     *     autocorrection (false). Note that autocorrection works only if the Omnibox is currently
     *     composing text.
     */
    public void commitText(@NonNull String textToCommit, boolean commitAsAutocomplete) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = mUrlBar.getInputConnection();
                    if (commitAsAutocomplete) conn.finishComposingText();
                    // Value of 1 always advance the cursor to the position after the full text
                    // being inserted.
                    conn.commitText(textToCommit, 1);
                });
    }

    /**
     * Specify the text to be offered as an inline autocompletion for the current user input.
     *
     * @param autocompleteText The suggested autocompletion for the text.
     * @param additionalText The additional autocompletion for the text.
     */
    public void setAutocompleteText(String autocompleteText, Optional<String> additionalText) {
        checkFocus(true);

        AtomicReference<String> userText = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    userText.set(mUrlBar.getTextWithoutAutocomplete());
                    mUrlBar.setAutocompleteText(userText.get(), autocompleteText, additionalText);
                });
        checkText(
                Matchers.equalTo(userText.get()),
                Matchers.equalTo(userText.get() + autocompleteText));
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     */
    public void checkText(
            @NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher) {
        checkText(textMatcher, autocompleteTextMatcher, null);
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     * @param additionalTextMatcher Optional Matcher for additional text.
     */
    public void checkText(
            @NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher) {
        checkText(textMatcher, autocompleteTextMatcher, additionalTextMatcher, null, null);
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     * @param additionalTextMatcher Optional Matcher for additional text.
     * @param autocompleteSelectionStart Matcher for Autocomplete's start position.
     * @param autocompleteSelectionEnd Matcher for Autocomplete's end position.
     */
    public void checkText(
            @NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher,
            int autocompleteSelectionStart,
            int autocompleteSelectionEnd) {
        checkText(
                textMatcher,
                autocompleteTextMatcher,
                additionalTextMatcher,
                Matchers.is(autocompleteSelectionStart),
                Matchers.is(autocompleteSelectionEnd));
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     * @param additionalTextMatcher Optional Matcher for additional text.
     * @param autocompleteSelectionStart Optional Matcher for Autocomplete's start position.
     * @param autocompleteSelectionEnd Optional Matcher for Autocomplete's end position.
     */
    public void checkText(
            @NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<String> additionalTextMatcher,
            @Nullable Matcher<Integer> autocompleteSelectionStart,
            @Nullable Matcher<Integer> autocompleteSelectionEnd) {
        waitAnimationsComplete();
        CriteriaHelper.pollUiThread(
                () -> {
                    if (mUrlBar.hasFocus()) {
                        // URL bar is focused. Match against the edit state.
                        Criteria.checkThat(
                                "Text without autocomplete should match",
                                mUrlBar.getTextWithoutAutocomplete(),
                                textMatcher);

                        Criteria.checkThat(
                                "Unexpected Autocomplete state",
                                mUrlBar.hasAutocomplete(),
                                Matchers.is(autocompleteTextMatcher != null));

                        if (autocompleteTextMatcher != null) {
                            Criteria.checkThat(
                                    "Text with autocomplete should match",
                                    mUrlBar.getTextWithAutocomplete(),
                                    autocompleteTextMatcher);
                        }

                        if (additionalTextMatcher != null) {
                            Criteria.checkThat(
                                    "Additional Text should match",
                                    mUrlBar.getAdditionalText().orElse(""),
                                    additionalTextMatcher);
                        }

                        if (autocompleteSelectionStart != null) {
                            Criteria.checkThat(
                                    "Autocomplete Selection start",
                                    mUrlBar.getSelectionStart(),
                                    autocompleteSelectionStart);
                        }

                        // TODO(crbug.com/40211958): Investigate why AutocompleteSelectionEnd was
                        // never enforced and why it doesn't work, then possibly re-enable the
                        // logic below: if (autocompleteSelectionEnd != null) {
                        //     Criteria.checkThat("Autocomplete Selection end",
                        //             mUrlBar.getSelectionEnd(),
                        //             autocompleteSelectionEnd);
                        // }
                    } else {
                        // URL bar is not focused. Match against the content.
                        Criteria.checkThat(mUrlBar.getText().toString(), textMatcher);
                    }
                });
    }

    /**
     * @return The text contents of the omnibox (without the Autocomplete part).
     */
    public String getText() {
        return ThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.getTextWithoutAutocomplete());
    }

    /**
     * Verify the Composing text in the Omnibox.
     *
     * Unlike Autocomplete, Composing text enables more finegrained control of the edited text.
     * This is particularly relevant to certain family of languages and input connections, where
     * individual characters or sequences are modified with subsequent keystrokes (eg. T9).
     *
     * @param textMatcher Matcher for the Omnibox content containing composed text.
     * @param composingRangeStart Character index where the compose begins.
     * @param composingRangeEnd Character index where the compose ends.
     */
    public void checkComposingText(
            @NonNull Matcher<String> textMatcher, int composingRangeStart, int composingRangeEnd) {
        checkFocus(true);
        CriteriaHelper.pollUiThread(
                () -> {
                    // Confirm no autocompletion during active compose.
                    Criteria.checkThat(
                            "Composing text should have no Autocompletion",
                            mUrlBar.hasAutocomplete(),
                            Matchers.is(false));

                    // Here getTextWithAutocomplete, getTextWithoutAutocomplete and getText should
                    // all return the same content. Since we already know there's no
                    // autocompletion, we can skip the additional validation.
                    Editable composingText = mUrlBar.getText();
                    Criteria.checkThat(composingText.toString(), textMatcher);

                    Criteria.checkThat(
                            "Composing Span Start",
                            BaseInputConnection.getComposingSpanStart(composingText),
                            Matchers.is(composingRangeStart));
                    Criteria.checkThat(
                            "Composing Span End",
                            BaseInputConnection.getComposingSpanEnd(composingText),
                            Matchers.is(composingRangeEnd));
                });
    }

    /**
     * Set the Composing text in the Omnibox.
     *
     * <p>Assumes that the supplied composingRegionStart is a valid text position (does not verify
     * test's sanity).
     *
     * @param composingText The composing text to apply.
     * @param composingRegionStart The placement inside the existing text where composing starts.
     * @param composingRegionEnd The placement inside the existing text where composing ends.
     */
    public void setComposingText(
            @NonNull String composingText, int composingRegionStart, int composingRegionEnd) {
        checkFocus(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InputConnection conn = mUrlBar.getInputConnection();
                    conn.setComposingRegion(composingRegionStart, composingRegionEnd);
                    conn.setComposingText(
                            composingText, /* newCursorPosition= */ composingText.length());
                });
    }

    /**
     * Click the n-th action.
     *
     * @param suggestionIndex the index of suggestion to click an action on.
     * @param actionIndex the index of action to invoke.
     */
    public void clickOnAction(int suggestionIndex, int actionIndex) {
        onView(withId(R.id.omnibox_suggestions_dropdown))
                .perform(
                        actionOnItemAtPosition(
                                suggestionIndex,
                                OmniboxTestUtils.actionOnOmniboxActionAtPosition(
                                        actionIndex, click())));
    }
}
