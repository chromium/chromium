// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spannable;

import androidx.annotation.DrawableRes;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AnswerTextStyle;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.SuggestionAnswer.ImageLine;
import org.chromium.components.omnibox.SuggestionAnswer.TextField;
import org.chromium.components.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Locale;

/**
 * Tests for {@link AnswerSuggestionProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.SUGGESTION_ANSWERS_COLOR_REVERSE})
public class AnswerSuggestionProcessorUnitTest {
    private static final @AnswerType int ANSWER_TYPES[] = {AnswerType.DICTIONARY,
            AnswerType.FINANCE, AnswerType.KNOWLEDGE_GRAPH, AnswerType.SPORTS, AnswerType.SUNRISE,
            AnswerType.TRANSLATION, AnswerType.WEATHER, AnswerType.WHEN_IS, AnswerType.CURRENCY};

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule TestRule mFeatureProcessor = new Features.JUnitProcessor();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock UrlBarEditingTextStateProvider mUrlStateProvider;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Bitmap mBitmap;

    private AnswerSuggestionProcessor mProcessor;
    private Locale mDefaultLocale;

    /**
     * Base Suggestion class that can be used for testing.
     * Holds all mechanisms that are required to processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        // Stores created AutocompleteMatch
        protected final AutocompleteMatch mSuggestion;
        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;
        // Stores Answer object associated with AutocompleteMatch (if any).
        private final SuggestionAnswer mAnswer;

        private SuggestionTestHelper(AutocompleteMatch suggestion, SuggestionAnswer answer,
                PropertyModel model, String userQuery) {
            mSuggestion = suggestion;
            mAnswer = answer;
            mModel = model;
            when(mUrlStateProvider.getTextWithoutAutocomplete()).thenReturn(userQuery);
            mProcessor.populateModel(mSuggestion, mModel, 0);
        }

        /** Check the content of first suggestion line. */
        private void verifyLine(String expectedTitle, int expectedMaxLineCount,
                String expectedDescription, WritableObjectPropertyKey<Spannable> titleKey,
                WritableIntPropertyKey maxLineCountKey,
                WritableObjectPropertyKey<String> descriptionKey) {
            final Spannable actualTitleSpan = mModel.get(titleKey);
            final String actualTitle = actualTitleSpan == null ? null : actualTitleSpan.toString();
            final String actualDescription = mModel.get(descriptionKey);

            Assert.assertNotNull(actualTitle);
            Assert.assertEquals(expectedTitle, actualTitle);

            Assert.assertEquals(expectedDescription, actualDescription);
            Assert.assertEquals(expectedMaxLineCount, mModel.get(maxLineCountKey));
        }

        void verifyLine1(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(expectedTitle, expectedMaxLineCount, expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION);
        }

        /** Check the content of second suggestion line. */
        void verifyLine2(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(expectedTitle, expectedMaxLineCount, expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION);
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final OmniboxDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            return state == null ? null : state.drawable;
        }

        @DrawableRes
        int getIconRes() {
            return shadowOf(getIcon()).getCreatedFromResId();
        }
    }

    /** Create Calculation Suggestion. */
    SuggestionTestHelper createCalculationSuggestion(String displayText, String userQuery) {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.CALCULATOR)
                        .setDisplayText(displayText)
                        .setDescription(userQuery)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, null, model, userQuery);
    }

    /** Create Answer Suggestion. */
    SuggestionTestHelper createAnswerSuggestion(@AnswerType int type, String line1Text,
            int line1Size, String line2Text, int line2Size, String url) {
        SuggestionAnswer answer =
                new SuggestionAnswer(type, createAnswerImageLine(line1Text, line1Size, null),
                        createAnswerImageLine(line2Text, line2Size, url));
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setAnswer(answer)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, answer, model, null);
    }

    /** Create a (possibly) multi-line ImageLine object with default formatting. */
    private ImageLine createAnswerImageLine(String text, int lines, String url) {
        return new ImageLine(Arrays.asList(new TextField(AnswerTextType.SUGGESTION, text,
                                     AnswerTextStyle.NORMAL, lines)),
                /* additionalText */ null, /* statusText */ null, url);
    }

    @Before
    public void setUp() {
        mProcessor = new AnswerSuggestionProcessor(ContextUtils.getApplicationContext(),
                mSuggestionHost, mUrlStateProvider, mImageSupplier);
        mDefaultLocale = Locale.getDefault();
    }

    @After
    public void tearDown() {
        Locale.setDefault(mDefaultLocale);
    }

    @Test
    public void regularAnswer_order() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "Query", 1, "Answer", 1, null);

        // Note: Most answers are shown in Answer -> Question order, but announced in
        // Question -> Answer order.
        suggHelper.verifyLine1("Answer", 1, "Query");
        suggHelper.verifyLine2("Query", 1, "Answer");
    }

    @Test
    public void dictionaryAnswer_order() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "Query", 1, "Answer", 1, null);

        // Note: Dictionary answers are shown in Question -> Answer order.
        suggHelper.verifyLine1("Query", 1, "Query");
        suggHelper.verifyLine2("Answer", 1, "Answer");
    }

    @Test
    public void calculationAnswer_order() {
        final SuggestionTestHelper suggHelper = createCalculationSuggestion("12345", "123 + 45");

        suggHelper.verifyLine1("123 + 45", 1, null);
        suggHelper.verifyLine2("12345", 1, null);
    }

    @Test
    public void regularAnswer_shortMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 1, "", 3, null);

        suggHelper.verifyLine1("", 3, "");
        suggHelper.verifyLine2("", 1, "");
    }

    @Test
    public void dictionaryAnswer_shortMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 3, null);

        suggHelper.verifyLine1("", 1, "");
        suggHelper.verifyLine2("", 3, "");
    }

    // Check that multiline answers that span across more than 3 lines - are reduced to 3 lines.
    // Check that multiline titles are truncated to a single line.
    @Test
    public void regularAnswer_truncatedMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 3, "", 10, null);

        suggHelper.verifyLine1("", 3, "");
        suggHelper.verifyLine2("", 1, "");
    }

    @Test
    public void dictionaryAnswer_truncatedMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 3, "", 10, null);

        suggHelper.verifyLine1("", 1, "");
        suggHelper.verifyLine2("", 3, "");
    }

    // Image fetching and icon association tests.
    @Test
    public void answerImage_fallbackIcons() {
        for (@AnswerType int type : ANSWER_TYPES) {
            SuggestionTestHelper suggHelper = createAnswerSuggestion(type, "", 1, "", 1, null);
            // Note: model is re-created on every iteration.
            Assert.assertNotNull("No icon associated with type: " + type, suggHelper.getIcon());
        }
    }

    @Test
    public void answerImage_iconAssociation() {
        SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_book_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.FINANCE, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_swap_vert_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_google_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.SPORTS, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_google_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.SUNRISE, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_wb_sunny_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.TRANSLATION, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.logo_translate_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.logo_partly_cloudy, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.WHEN_IS, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_event_round, suggHelper.getIconRes());

        suggHelper = createAnswerSuggestion(AnswerType.CURRENCY, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_loop_round, suggHelper.getIconRes());
    }

    @Test
    public void answerImage_calculatorIcon() {
        var suggHelper = createCalculationSuggestion("", "");
        Assert.assertEquals(R.drawable.ic_equals_sign_round, suggHelper.getIconRes());
    }

    @Test
    public void answerImage_fallbackIconServedForUnsupportedAnswerType() {
        var suggHelper = createAnswerSuggestion(AnswerType.TOTAL_COUNT, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_suggestion_magnifier, suggHelper.getIconRes());
    }

    @Test
    public void answerImage_noImageFetchWhenFetcherIsUnavailable() {
        final String url = "http://site.com";
        mProcessor = new AnswerSuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, mUrlStateProvider, null);
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url);
        Assert.assertNotNull(suggHelper.getIcon());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SUGGESTION_ANSWERS_COLOR_REVERSE)
    public void checkColorReversalRequired_ReturnsFalseIfOmniBoxAnswerColorReversalDisabled() {
        mProcessor.onNativeInitialized();
        for (@AnswerType int type : ANSWER_TYPES) {
            Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
        }
    }

    @Test
    public void
    checkColorReversalRequired_ReturnsTrueIfOmniBoxAnswerColorReversalEnabledAndIncludedInCountryList() {
        mProcessor.onNativeInitialized();
        Locale.setDefault(new Locale("ja", "JP"));
        for (@AnswerType int type : ANSWER_TYPES) {
            if (type == AnswerType.FINANCE) {
                Assert.assertTrue(mProcessor.checkColorReversalRequired(type));
            } else {
                Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
            }
        }
    }

    @Test
    public void
    checkColorReversalRequired_ReturnsFalseIfOmniBoxAnswerColorReversalEnabledAndNotIncludedInCountryList() {
        mProcessor.onNativeInitialized();
        Locale.setDefault(new Locale("en", "US"));
        for (@AnswerType int type : ANSWER_TYPES) {
            Assert.assertFalse(mProcessor.checkColorReversalRequired(type));
        }
    }

    @Test
    public void doesProcessSuggestion_suggestionWithAnswer() {
        SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, null);
        Assert.assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_calculatorSuggestion() {
        SuggestionTestHelper suggHelper = createCalculationSuggestion("abcd", "efgh");
        Assert.assertTrue(mProcessor.doesProcessSuggestion(suggHelper.mSuggestion, 0));
    }

    @Test
    public void doesProcessSuggestion_ignoreNonCalculatorSuggestionsWithNoAnswers() {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        Assert.assertFalse(mProcessor.doesProcessSuggestion(suggestion, 0));
    }

    @Test
    public void fetchAnswerImage_withSupplier() {
        var suggHelper =
                createAnswerSuggestion(AnswerType.TOTAL_COUNT, "", 1, "", 1, JUnitTestGURLs.BLUE_1);

        ArgumentCaptor<Callback<Bitmap>> cb = ArgumentCaptor.forClass(Callback.class);
        verify(mImageSupplier, times(1))
                .fetchImage(eq(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1)), cb.capture());

        var sds1 = suggHelper.mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(sds1);

        cb.getValue().onResult(mBitmap);

        var sds2 = suggHelper.mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(sds2);
        Assert.assertNotEquals(sds1, sds2);
        Assert.assertTrue(sds2.drawable instanceof BitmapDrawable);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) sds2.drawable).getBitmap());
    }

    @Test
    public void fetchAnswerImage_withoutSupplier() {
        mProcessor = new AnswerSuggestionProcessor(ContextUtils.getApplicationContext(),
                mSuggestionHost, mUrlStateProvider, /* imageSupplier=*/null);

        var suggHelper =
                createAnswerSuggestion(AnswerType.TOTAL_COUNT, "", 1, "", 1, JUnitTestGURLs.BLUE_1);

        var sds1 = suggHelper.mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(sds1);
        verifyNoMoreInteractions(mImageSupplier);
    }

    @Test
    public void getViewTypeId_forFullTestCoverage() {
        Assert.assertEquals(OmniboxSuggestionUiType.ANSWER_SUGGESTION, mProcessor.getViewTypeId());
    }
}
