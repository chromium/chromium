// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.text.Spannable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteUIContext;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.function.Supplier;

/** Tests for {@link AnswerSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AnswerSuggestionProcessorUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private SuggestionHost mSuggestionHost;
    @Mock private UrlBarEditingTextStateProvider mUrlStateProvider;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private OmniboxActionDelegate mActionDelegate;

    private Activity mContext;
    private AnswerSuggestionProcessor mProcessor;
    private AutocompleteInput mInput;

    /**
     * Base Suggestion class that can be used for testing. Holds all mechanisms that are required to
     * processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        final AutocompleteMatch mSuggestion;
        final PropertyModel mModel;

        private SuggestionTestHelper(
                AutocompleteMatch suggestion, PropertyModel model, String userQuery) {
            mSuggestion = suggestion;
            mModel = model;

            when(mUrlStateProvider.getTextWithoutAutocomplete()).thenReturn(userQuery);
            mProcessor.populateModel(mInput, mSuggestion, mModel, 0);
        }

        private void verifyLine(
                String expectedTitle,
                int expectedMaxLineCount,
                String expectedDescription,
                WritableObjectPropertyKey<Spannable> titleKey,
                WritableIntPropertyKey maxLineCountKey,
                WritableObjectPropertyKey<String> descriptionKey) {
            final Spannable actualTitleSpan = mModel.get(titleKey);
            final String actualTitle = actualTitleSpan == null ? null : actualTitleSpan.toString();
            final String actualDescription = mModel.get(descriptionKey);

            assertNotNull(actualTitle);
            assertEquals(expectedTitle, actualTitle);

            assertEquals(expectedDescription, actualDescription);
            assertEquals(expectedMaxLineCount, mModel.get(maxLineCountKey));
        }

        void verifyLine1(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(
                    expectedTitle,
                    expectedMaxLineCount,
                    expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION);
        }

        void verifyLine2(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(
                    expectedTitle,
                    expectedMaxLineCount,
                    expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION);
        }

        Drawable getIcon() {
            final OmniboxDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            assertTrue(state.isLarge);
            return state == null ? null : state.drawable;
        }

        int getIconRes() {
            return shadowOf(getIcon()).getCreatedFromResId();
        }
    }

    SuggestionTestHelper createCalculationSuggestion(String displayText, String userQuery) {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.CALCULATOR)
                        .setDisplayText(displayText)
                        .setDescription(userQuery)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model, userQuery);
    }

    @Before
    public void setUp() {
        mContext = Robolectric.buildActivity(Activity.class).setup().get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        AutocompleteUIContext uiContext =
                new AutocompleteUIContext(
                        mContext,
                        mSuggestionHost,
                        mUrlStateProvider,
                        /* imageSupplier= */ null,
                        /* bookmarkState= */ null,
                        /* activityTabSupplier= */ null,
                        mShareDelegateSupplier,
                        ObservableSuppliers.createNonNull(ControlsPosition.TOP),
                        mActionDelegate);
        mProcessor = new AnswerSuggestionProcessor(uiContext);
        mInput = new AutocompleteInput();
    }

    @Test
    public void calculationAnswer_order() {
        final SuggestionTestHelper suggHelper = createCalculationSuggestion("12345", "123 + 45");

        suggHelper.verifyLine1("123 + 45", 1, null);
        suggHelper.verifyLine2("12345", 1, null);
    }

    @Test
    public void answerImage_calculatorIcon() {
        var suggHelper = createCalculationSuggestion("", "");
        assertEquals(R.drawable.ic_equals_sign_round, suggHelper.getIconRes());
    }

    @Test
    public void doesProcessSuggestion_calculatorSuggestion() {
        SuggestionTestHelper suggHelper = createCalculationSuggestion("abcd", "efgh");
        assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_ignoreNonCalculatorSuggestionsWithNoAnswers() {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        assertFalse(mProcessor.doesProcessSuggestion(suggestion, 0));
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        assertEquals(OmniboxSuggestionUiType.ANSWER_SUGGESTION, mProcessor.getViewTypeId());
    }
}
