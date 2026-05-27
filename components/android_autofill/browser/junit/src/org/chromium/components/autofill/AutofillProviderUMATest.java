// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import static org.junit.Assert.assertEquals;

import static org.chromium.components.autofill.AutofillProviderUMA.toCreationContext;

import static java.util.Collections.singletonList;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.autofill.AutofillProviderUMA.AutofillManagerCreationContext;
import org.chromium.components.autofill.AutofillProviderUMA.AutofillManagerMethod;
import org.chromium.components.autofill.AutofillProviderUMA.Provider;

/** Tests for {@link AutofillProviderUMA} general metrics. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class AutofillProviderUMATest {

    private AutofillProviderUMA mAutofillUMA;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mAutofillUMA =
                new AutofillProviderUMA(
                        mContext,
                        /* isAwGCurrentAutofillService= */ true,
                        /* packageName= */ "com.google.android.gms");
    }

    @Test
    public void testAutofillEnabled() {
        // Test first call
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(AutofillProviderUMA.UMA_AUTOFILL_ENABLED, true)
                        .build();
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        watcher.assertExpected();

        // Test no record if value doesn't change
        watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(AutofillProviderUMA.UMA_AUTOFILL_ENABLED)
                        .build();
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        watcher.assertExpected();

        // Test record when value changes
        watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(AutofillProviderUMA.UMA_AUTOFILL_ENABLED, false)
                        .build();
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ true);
        watcher.assertExpected();
    }

    @Test
    public void testToCreationContext() {
        assertEquals(AutofillManagerCreationContext.NULL, toCreationContext(null));

        Context appContext = ContextUtils.getApplicationContext();
        assertEquals(
                AutofillManagerCreationContext.APPLICATION_CONTEXT, toCreationContext(appContext));

        Context unknownContext = new ContextWrapper(ContextUtils.getApplicationContext());
        assertEquals(AutofillManagerCreationContext.UNKNOWN, toCreationContext(unknownContext));

        Activity activity = Robolectric.setupActivity(Activity.class);
        assertEquals(AutofillManagerCreationContext.ACTIVITY_CONTEXT, toCreationContext(activity));
    }

    @Test
    public void testRecordUmaCreationContext() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_CREATION_CONTEXT,
                                AutofillManagerCreationContext.NULL)
                        .build();
        AutofillProviderUMA.recordUmaCreationContext(null);
        watcher.assertExpected();
    }

    @Test
    public void testSubmissionSourceMapping() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                AutofillProviderUMA.SAME_DOCUMENT_NAVIGATION)
                        .build();
        mAutofillUMA.onFormSubmitted(SubmissionSource.SAME_DOCUMENT_NAVIGATION);
        watcher.assertExpected();

        watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                AutofillProviderUMA.FORM_SUBMISSION)
                        .build();
        mAutofillUMA.onFormSubmitted(SubmissionSource.FORM_SUBMISSION);
        watcher.assertExpected();
    }

    @Test
    public void testSuggestionTime() {
        final var suggestionTimeMillis = 500;
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);
        mAutofillUMA.onVirtualStructureProvided();
        mAutofillUMA.onSuggestionDisplayed(suggestionTimeMillis);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SUGGESTION_TIME,
                                suggestionTimeMillis)
                        .build();
        mAutofillUMA.recordSession();
        watcher.assertExpected();
    }

    @Test
    public void testGetCurrentProvider() {
        assertEquals(
                Provider.AWG, AutofillProviderUMA.getCurrentProvider("com.google.android.gms"));
        assertEquals(
                Provider.SAMSUNG_PASS,
                AutofillProviderUMA.getCurrentProvider("com.samsung.android.samsungpassautofill"));
        assertEquals(Provider.UNKNOWN, AutofillProviderUMA.getCurrentProvider("unknown.package"));
    }

    @Test
    public void testLogCurrentProvider() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(AutofillProviderUMA.UMA_AUTOFILL_PROVIDER, Provider.AWG)
                        .build();
        AutofillProviderUMA.logCurrentProvider("com.google.android.gms");
        watcher.assertExpected();
    }

    @Test
    public void testRecordException() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_MANAGER_ERROR,
                                AutofillManagerMethod.COMMIT)
                        .build();
        AutofillProviderUMA.recordException(new Exception("test"), AutofillManagerMethod.COMMIT);
        watcher.assertExpected();
    }

    @Test
    public void testServerPredictionAvailableOnStart() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                AutofillProviderUMA.SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS)
                        .build();
        mAutofillUMA.onServerTypeAvailable(null, /* afterSessionStarted= */ false);
        watcher.assertExpected();
    }

    @Test
    public void testServerPredictionAvailableAfterStart() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                AutofillProviderUMA
                                        .SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS)
                        .build();
        mAutofillUMA.onServerTypeAvailable(null, /* afterSessionStarted= */ true);
        watcher.assertExpected();
    }

    @Test
    public void testServerPredictionNotAvailable() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                AutofillProviderUMA.SERVER_PREDICTION_NOT_AVAILABLE)
                        .build();
        mAutofillUMA.recordSession();
        watcher.assertExpected();
    }

    @Test
    public void testServerPrediction_HasValidServerPrediction() {
        mAutofillUMA.onSessionStarted(/* autofillDisabled= */ false);

        FormFieldDataBuilder fieldBuilder = new FormFieldDataBuilder();
        fieldBuilder.mServerType = "EMAIL_ADDRESS";
        FormData formData =
                new FormData(
                        /* sessionId= */ 1,
                        "FormName",
                        "https://example.com",
                        singletonList(fieldBuilder.build()));

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                AutofillProviderUMA.UMA_AUTOFILL_VALID_SERVER_PREDICTION, true)
                        .allowExtraRecords(
                                AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY)
                        .build();
        mAutofillUMA.onServerTypeAvailable(formData, /* afterSessionStarted= */ false);
        watcher.assertExpected();
    }
}
