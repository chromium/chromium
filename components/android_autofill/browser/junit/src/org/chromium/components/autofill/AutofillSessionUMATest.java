// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.HistogramWatcher;

import java.util.Arrays;
import java.util.Collection;

/**
 * Tests that the "Autofill.WebView.AutofillSession(WithBottomSheet)" histograms are recorded
 * correctly inside `AutofillProviderUMA`.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillSessionUMATest {
    private static final String NO_SUGGESTION = "NO_SUGGESTION";
    private static final String USER_SELECT_SUGGESTION = "USER_SELECT_SUGGESTION";
    private static final String USER_NOT_SELECT_SUGGESTION = "USER_NOT_SELECT_SUGGESTION";

    @Parameters(name = "{0}")
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {
                    {NO_SUGGESTION}, {USER_SELECT_SUGGESTION}, {USER_NOT_SELECT_SUGGESTION}
                });
    }

    @Parameter(0)
    public String mSuggestionInteractionType;

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private AutofillProviderUMA mAutofillUMA;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = Mockito.mock(Context.class);
        mAutofillUMA =
                new AutofillProviderUMA(
                        mContext, /* isAwGCurrentAutofillService= */ true, /* packageName= */ "");
    }

    /** Tests that the "*_USER_CHANGE_FORM_FORM_SUBMITTED" buckets are emitted. */
    @Test
    public void testUserChangeFormFormSubmitted() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        mAutofillUMA.onBottomSheetShown();

        int histogramBucket = 0;
        switch (mSuggestionInteractionType) {
            case NO_SUGGESTION:
                histogramBucket = AutofillProviderUMA.NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
                break;
            case USER_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA.USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                mAutofillUMA.onAutofill();
                break;
            case USER_NOT_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                break;
            default:
                throw new AssertionError(); // NOTREACHED()
        }

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION, histogramBucket)
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET,
                                histogramBucket)
                        .build();
        mAutofillUMA.onUserChangeFieldValue(/* isPreviouslyAutofilled= */ false);
        mAutofillUMA.onFormSubmitted(SubmissionSource.FORM_SUBMISSION);
        histogramWatcher.assertExpected();
    }

    /** Tests that the "*_USER_CHANGE_FORM_NO_FORM_SUBMITTED" buckets are emitted. */
    @Test
    public void testUserChangeFormNoFormSubmitted() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        mAutofillUMA.onBottomSheetShown();

        int histogramBucket = 0;
        switch (mSuggestionInteractionType) {
            case NO_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA.NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
                break;
            case USER_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                mAutofillUMA.onAutofill();
                break;
            case USER_NOT_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                break;
            default:
                throw new AssertionError(); // NOTREACHED()
        }

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION, histogramBucket)
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET,
                                histogramBucket)
                        .build();
        mAutofillUMA.onUserChangeFieldValue(/* isPreviouslyAutofilled= */ false);
        mAutofillUMA.recordSession();
        histogramWatcher.assertExpected();
    }

    /** Tests that the "*_USER_NOT_CHANGE_FORM_FORM_SUBMITTED" buckets are emitted. */
    @Test
    public void testUserNotChangeFormFormSubmitted() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        mAutofillUMA.onBottomSheetShown();

        int histogramBucket = 0;
        switch (mSuggestionInteractionType) {
            case NO_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
                break;
            case USER_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                mAutofillUMA.onAutofill();
                break;
            case USER_NOT_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                break;
            default:
                throw new AssertionError(); // NOTREACHED()
        }

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION, histogramBucket)
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET,
                                histogramBucket)
                        .build();
        mAutofillUMA.onFormSubmitted(SubmissionSource.FORM_SUBMISSION);
        histogramWatcher.assertExpected();
    }

    /** Tests that the "*_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED" buckets are emitted. */
    @Test
    public void testUserNotChangeFormNoFormSubmitted() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        mAutofillUMA.onBottomSheetShown();

        int histogramBucket = 0;
        switch (mSuggestionInteractionType) {
            case NO_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA.NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
                break;
            case USER_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                mAutofillUMA.onAutofill();
                break;
            case USER_NOT_SELECT_SUGGESTION:
                histogramBucket =
                        AutofillProviderUMA
                                .USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED;
                mAutofillUMA.onSuggestionDisplayed(/* suggestionTimeMillis= */ 0);
                break;
            default:
                throw new AssertionError(); // NOTREACHED()
        }

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION, histogramBucket)
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION_WITH_BOTTOM_SHEET,
                                histogramBucket)
                        .build();
        mAutofillUMA.recordSession();
        histogramWatcher.assertExpected();
    }
}
