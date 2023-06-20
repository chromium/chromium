// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController.OnSuggestionsReceivedListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemViewInfo;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdown;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownAdapter;
import org.chromium.chrome.browser.omnibox.suggestions.base.ActionChipsProperties;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderView;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Function;

/**
 * Utility methods and classes for testing the Omnibox.
 */
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
     * This class can be chained with {@link
     * androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition}.
     */
    private static class ActionOnOmniobxActionAtPosition implements ViewAction {
        private final ViewAction mAction;

        public ActionOnOmniobxActionAtPosition(int position, ViewAction action) {
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
        return new ActionOnOmniobxActionAtPosition(position, action);
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

        protected SuggestionInfo(int index, @OmniboxSuggestionUiType int type,
                @NonNull AutocompleteMatch suggestion, @NonNull PropertyModel model,
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

    /**
     * Disables any live autocompletion, making Omnibox behave like a standard text field.
     */
    public void disableLiveAutocompletion() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mUrlBar.setUrlTextChangeListener(null));
    }

    /**
     * Waits for all the animations to complete.
     * Allows any preceding operation to kick off an animation.
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
     * Note: this is known to cause issues with tests that run animations.
     * In the event you are running into flakes that concentrate around this call, please consider
     * adding DisableAnimationsTestRule to your test suite.
     *
     * @param active Whether the Omnibox is expected to have focus or not.
     */
    public void checkFocus(boolean active) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    "unexpected Omnibox focus state", mUrlBar.hasFocus(), Matchers.is(active));
            InputMethodManager imm = (InputMethodManager) mUrlBar.getContext().getSystemService(
                    Context.INPUT_METHOD_SERVICE);
            Criteria.checkThat("Keyboard did not reach expected state", imm.isActive(mUrlBar),
                    Matchers.is(active));
        });
    }

    /**
     * Determines whether the UrlBar currently has focus.
     * @return Whether the UrlBar has focus.
     */
    public boolean getFocus() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> mUrlBar.hasFocus());
    }

    /**
     * Request the Omnibox focus and wait for soft keyboard to show.
     */
    public void requestFocus() {
        // During early startup (before completion of its first onDraw), the UrlBar
        // is not focusable. Tests have to wait for that to happen before trying to focus it.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("Omnibox not shown.", mUrlBar.isShown(), Matchers.is(true));
            Criteria.checkThat("Omnibox not focusable.", mUrlBar.isFocusable(), Matchers.is(true));
        });

        TestThreadUtils.runOnUiThreadBlockingNoException(() -> mUrlBar.requestFocus());
        waitAnimationsComplete();
        checkFocus(true);
    }

    /**
     * Clear the Omnibox focus and wait until keyboard is dismissed.
     * Expects the Omnibox to be focused before the call.
     */
    public void clearFocus() {
        sendKey(KeyEvent.KEYCODE_BACK);
        checkFocus(false);
    }

    /**
     * Set the suggestions to the Omnibox to display.
     *
     * @param autocompleteResult The set of suggestions will be displayed on the Omnibox dropdown
     *         list.
     * @param inlineAutocompleteText the inline-autocomplete text.
     */
    public void setSuggestions(
            AutocompleteResult autocompleteResult, String inlineAutocompleteText) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OnSuggestionsReceivedListener listener =
                    mAutocomplete.getSuggestionsReceivedListenerForTest();
            listener.onSuggestionsReceived(autocompleteResult, inlineAutocompleteText, true);
        });
    }

    /**
     * Waits for a non-empty list of omnibox suggestions is shown.
     */
    public void checkSuggestionsShown() {
        CriteriaHelper.pollUiThread(() -> {
            OmniboxSuggestionsDropdown suggestionsDropdown =
                    mLocationBar.getAutocompleteCoordinator().getSuggestionsDropdownForTest();
            Criteria.checkThat(
                    "suggestion list is null", suggestionsDropdown, Matchers.notNullValue());
            Criteria.checkThat("suggestion list is not shown",
                    suggestionsDropdown.getViewGroup().isShown(), Matchers.is(true));
            Criteria.checkThat("suggestion list has no entries",
                    suggestionsDropdown.getDropdownItemViewCountForTest(), Matchers.greaterThan(0));
        });
    }

    /**
     * Stops any subsequent AutocompleteResults from being generated.
     * Ensures that no subsequent asynchronous AutocompleteResults could tamper with test execution.
     */
    public void waitForAutocomplete() {
        AtomicLong previousId = new AtomicLong(-1);
        AtomicLong count = new AtomicLong();

        CriteriaHelper.pollUiThread(() -> {
            long currentId = mAutocomplete.getCurrentNativeAutocompleteResult();
            // Suggestions have changed as a result of a recent push.
            // Reset the counter and monitor for possible updates.
            if (currentId != previousId.get()) {
                previousId.set(currentId);
                count.set(0);
                return false;
            }

            // Check that nothing has changed 3 times in a row, rejecting everything that
            // arrives late. This guarantees that the suggestions will not change and the list
            // can be used for testing purposes.
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

    /**
     * Return the first suggestion that features Action Chips.
     */
    public @Nullable<T extends View> SuggestionInfo<T> findSuggestionWithActionChips() {
        return findSuggestion(info -> {
            if (!info.model.getAllSetProperties().contains(ActionChipsProperties.ACTION_CHIPS)) {
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
    public @Nullable<T extends View> SuggestionInfo<T> findSuggestion(
            @NonNull Function<DropdownItemViewInfo, Boolean> filter) {
        checkSuggestionsShown();
        AtomicReference<SuggestionInfo<T>> result = new AtomicReference<>();

        CriteriaHelper.pollUiThread(() -> {
            ModelList currentModels =
                    mLocationBar.getAutocompleteCoordinator().getSuggestionModelListForTest();
            for (int i = 0; i < currentModels.size(); i++) {
                DropdownItemViewInfo info = (DropdownItemViewInfo) currentModels.get(i);
                if (filter.apply(info) && getSuggestionViewForIndex(i) != null) {
                    result.set(new SuggestionInfo<T>(i, info.type, mAutocomplete.getSuggestionAt(i),
                            info.model, getSuggestionViewForIndex(i)));
                    return true;
                }
            }
            return false;
        });

        return result.get();
    }

    /**
     * Retrieve the Suggestion View for specific suggestion index.
     * Traverses the Suggestions list and skips over the Headers.
     *
     * @param <T> The type of the expected view. Inferred from call.
     * @param indexOfSuggestion The index of the suggestion view (not including the headers).
     * @return The View corresponding to suggestion with specific index, or null if there's no such
     *         suggestion.
     */
    private @Nullable<T extends View> T getSuggestionViewForIndex(int indexOfSuggestion) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            OmniboxSuggestionsDropdown dropdown =
                    mLocationBar.getAutocompleteCoordinator().getSuggestionsDropdownForTest();
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
     * Highligh suggestion at a specific index.
     *
     * @param index The index of the suggestion to be highlighted.
     */
    public void focusSuggestion(int index) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OmniboxSuggestionsDropdownAdapter adapter =
                    (OmniboxSuggestionsDropdownAdapter) mLocationBar.getAutocompleteCoordinator()
                            .getSuggestionsDropdownForTest()
                            .getAdapter();
            adapter.setSelectedViewIndex(index);
        });
    }

    /**
     * Type text in the Omnibox.
     * Requires that the Omnibox is focused ahead of call.
     *
     * @param text Text to be "typed" in the Omnibox.
     * @param execute Whether to perform the default action after typing text (ie. press the "go"
     *         button/enter key).
     */
    public void typeText(String text, boolean execute) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> KeyUtils.typeTextIntoView(mInstrumentation, mUrlBar, text));

        if (execute) sendKey(KeyEvent.KEYCODE_ENTER);
    }

    /**
     * Send key event to the Omnibox.
     * Requires that the Omnibox is focused.
     *
     * @param keyCode The Key code to send to the Omnibox.
     */
    public void sendKey(final int keyCode) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> KeyUtils.singleKeyEventView(mInstrumentation, mUrlBar, keyCode));
    }

    /**
     * Specify the text to be shown in the Omnibox. Cancels all autocompletion.
     * Use this to initialize the state of the Omnibox, but avoid using this to validate any
     * behavior.
     *
     * @param userText The text to be shown in the Omnibox.
     */
    public void setText(String userText) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUrlBar.setText(userText);
            // Push this to the model as well.
            mUrlBar.setAutocompleteText(userText, "");
        });
        checkText(Matchers.equalTo(userText), null);
    }

    /**
     * Commit text to the Omnibox, as if it was supplied by the soft keyboard
     * autocorrect/autocomplete feature.
     *
     * @param textToCommit The text to supply as if it was supplied by Soft Keyboard.
     * @param commitAsAutocomplete Whether the text should be applied as autocompletion (true) or
     *         autocorrection (false). Note that autocorrection works only if the Omnibox is
     *         currently composing text.
     */
    public void commitText(@NonNull String textToCommit, boolean commitAsAutocomplete) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InputConnection conn = mUrlBar.getInputConnection();
            if (commitAsAutocomplete) conn.finishComposingText();
            // Value of 1 always advance the cursor to the position after the full text being
            // inserted.
            conn.commitText(textToCommit, 1);
        });
    }

    /**
     * Specify the text to be offered as an inline autocompletion for the current user input.
     *
     * @param autocompleteText The suggested autocompletion for the text.
     */
    public void setAutocompleteText(String autocompleteText) {
        checkFocus(true);

        AtomicReference<String> userText = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            userText.set(mUrlBar.getTextWithoutAutocomplete());
            mUrlBar.setAutocompleteText(userText.get(), autocompleteText);
        });
        checkText(Matchers.equalTo(userText.get()),
                Matchers.equalTo(userText.get() + autocompleteText));
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     */
    public void checkText(@NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher) {
        checkText(textMatcher, autocompleteTextMatcher, null, null);
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     * @param autocompleteSelectionStart Matcher for Autocomplete's start position.
     * @param autocompleteSelectionEnd Matcher for Autocomplete's end position.
     */
    public void checkText(@NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher, int autocompleteSelectionStart,
            int autocompleteSelectionEnd) {
        checkText(textMatcher, autocompleteTextMatcher, Matchers.is(autocompleteSelectionStart),
                Matchers.is(autocompleteSelectionEnd));
    }

    /**
     * Verify the text content of the Omnibox.
     *
     * @param textMatcher Matcher checking the content of the Omnibox.
     * @param autocompleteTextMatcher Optional Matcher for autocompletion.
     * @param autocompleteSelectionStart Optional Matcher for Autocomplete's start position.
     * @param autocompleteSelectionEnd Optional Matcher for Autocomplete's end position.
     */
    public void checkText(@NonNull Matcher<String> textMatcher,
            @Nullable Matcher<String> autocompleteTextMatcher,
            @Nullable Matcher<Integer> autocompleteSelectionStart,
            @Nullable Matcher<Integer> autocompleteSelectionEnd) {
        waitAnimationsComplete();
        CriteriaHelper.pollUiThread(() -> {
            if (mUrlBar.hasFocus()) {
                // URL bar is focused. Match against the edit state.
                Criteria.checkThat("Text without autocomplete should match",
                        mUrlBar.getTextWithoutAutocomplete(), textMatcher);

                Criteria.checkThat("Unexpected Autocomplete state", mUrlBar.hasAutocomplete(),
                        Matchers.is(autocompleteTextMatcher != null));

                if (autocompleteTextMatcher != null) {
                    Criteria.checkThat("Text with autocomplete should match",
                            mUrlBar.getTextWithAutocomplete(), autocompleteTextMatcher);
                }

                if (autocompleteSelectionStart != null) {
                    Criteria.checkThat("Autocomplete Selection start", mUrlBar.getSelectionStart(),
                            autocompleteSelectionStart);
                }

                // TODO(crbug.com/1289474): Investigate why AutocompleteSelectionEnd was never
                // enforced and why it doesn't work, then possibly re-enable the logic below:
                // if (autocompleteSelectionEnd != null) {
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
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mUrlBar.getTextWithoutAutocomplete());
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
        CriteriaHelper.pollUiThread(() -> {
            // Confirm no autocompletion during active compose.
            Criteria.checkThat("Composing text should have no Autocompletion",
                    mUrlBar.hasAutocomplete(), Matchers.is(false));

            // Here getTextWithAutocomplete, getTextWithoutAutocomplete and getText should all
            // return the same content. Since we already know there's no autocompletion, we
            // can skip the additional validation.
            Editable composingText = mUrlBar.getText();
            Criteria.checkThat(composingText.toString(), textMatcher);

            Criteria.checkThat("Composing Span Start",
                    BaseInputConnection.getComposingSpanStart(composingText),
                    Matchers.is(composingRangeStart));
            Criteria.checkThat("Composing Span End",
                    BaseInputConnection.getComposingSpanEnd(composingText),
                    Matchers.is(composingRangeEnd));
        });
    }

    /**
     * Set the Composing text in the Omnibox.
     *
     * Assumes that the supplied composingRegionStart is a valid text position (does not verify
     * test's sanity).
     *
     * @param composingText The composing text to apply.
     * @param composingRegionStart The placement inside the existing text where composing starts.
     * @param composingRegionEnd The placement inside the existing text where composing ends.
     */
    public void setComposingText(
            @NonNull String composingText, int composingRegionStart, int composingRegionEnd) {
        checkFocus(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InputConnection conn = mUrlBar.getInputConnection();
            conn.setComposingRegion(composingRegionStart, composingRegionEnd);
            conn.setComposingText(composingText, /* newCursorPosition=*/0);
        });
    }
}
